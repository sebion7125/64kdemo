// trees_cpu.c - prozedurale Baum-Geometrie (reines C, kein GL).
//
// Ersetzt die alten SDF-Baeume (Stamm + 6 Kugeln) durch echte Struktur:
//   - Stamm: gebogener, konischer Turm mit Wurzelflan
//   - 5 Hauptaeste (2 Segmente, gekruemmt) + 10 Seitenaeste
//   - 240-360 individuelle Blaetter pro Baum (Instanzen):
//     45% in Clustern an den Astspitzen, 55% im Kronen-Ellipsoid
//   - Kronen-Billboard (2 gekreuzte Quads) als Distanz-LOD
//   - Wireframe-Skelett (Taste T) aus den Ast-Tubes
//
// Platzierung identisch zum alten SDF-System (gleiche Zellen, Hashes,
// Terrain-Filter) -> Baeume stehen an den selben Positionen wie zuvor.
#include "scene.h"
#include <math.h>
#ifdef TREE_SEAM_DEBUG
#include <stdio.h>
#endif
#include <string.h>

float treeVerts[TREE_VERT_CAP * TREE_FLOATS];
float crownVerts[CROWN_CAP * CROWN_FLOATS];
float leafInst[LEAF_CAP * LEAF_FLOATS];
float wireVerts[WIRE_CAP * WIRE_FLOATS];
int nTreeVerts = 0, nCrownVerts = 0, nWireVerts = 0, nTrees = 0, nLeafInst = 0;

// ---------- kleine Helfer ----------
static void add_wire(float x0, float y0, float z0, float x1, float y1, float z1){
  if(nWireVerts + 2 > WIRE_CAP) return;
  wireVerts[nWireVerts * WIRE_FLOATS + 0] = x0;
  wireVerts[nWireVerts * WIRE_FLOATS + 1] = y0;
  wireVerts[nWireVerts * WIRE_FLOATS + 2] = z0;
  wireVerts[(nWireVerts + 1) * WIRE_FLOATS + 0] = x1;
  wireVerts[(nWireVerts + 1) * WIRE_FLOATS + 1] = y1;
  wireVerts[(nWireVerts + 1) * WIRE_FLOATS + 2] = z1;
  nWireVerts += 2;
}

// ---------- Rohr mit parallel transportierten Rahmen ----------
// Das alte emit_tube baute pro Segment eine EIGENE orthogonale Basis aus der
// Achsenrichtung (up-Heuristik). An einem Gelenk (Segmentende = naechstes
// Segmentanfang) stehen dieselben Punkte, aber die Ringe sind um verschiedene
// Winkel gedreht -> die Quad-Randen teilen sich nicht -> offene Nahte/Schlitze
// im Stamm, durch die man den Hintergrund sieht.
// Jetzt: ein Ring pro Knoten, und der naechste Ring wird durch Parallel-
// Transport (Drehung um cross(d0,d1)) aus dem vorherigen gewonnen. Damit sind
// die beiden Ringe an einem Gelenk IDENTISCH -> dichtes Rohr.
typedef struct { float p[3]; float r; float u[3], v[3]; } Ring;

// Basis aus einer Achsenrichtung (nur fuer den ersten Ring einer Kette)
static void ring_basis(Ring *rg, float ax, float ay, float az){
  float ux, uy, uz;
  if(fabsf(ay) < 0.9f){ ux = -az; uy = 0.0f; uz = ax; }   // cross(axis,(0,1,0))
  else                { ux = 0.0f; uy = az; uz = -ay; }   // cross(axis,(1,0,0))
  float ul = sqrtf(ux * ux + uy * uy + uz * uz);
  if(ul < 1e-6f){ ux = 1.0f; uy = 0.0f; uz = 0.0f; ul = 1.0f; }
  rg->u[0] = ux / ul; rg->u[1] = 0.0f; rg->u[2] = uz / ul;
  rg->v[0] = ay * rg->u[2] - az * rg->u[1];
  rg->v[1] = az * rg->u[0] - ax * rg->u[2];
  rg->v[2] = ax * rg->u[1] - ay * rg->u[0];
}

// Rahmen von Achse d0 auf Achse d1 parallel transportieren (Rodrigues).
static void ring_transport(Ring *out, const Ring *in,
                           float ax0, float ay0, float az0,
                           float ax1, float ay1, float az1){
  float cx = ay0 * az1 - az0 * ay1;
  float cy = az0 * ax1 - ax0 * az1;
  float cz = ax0 * ay1 - ay0 * ax1;
  float s = sqrtf(cx * cx + cy * cy + cz * cz);
  float c = ax0 * ax1 + ay0 * ay1 + az0 * az1;
  float ux = in->u[0], uy = in->u[1], uz = in->u[2];
  if(s > 1e-6f){
    float kx = cx / s, ky = cy / s, kz = cz / s;
    float ct = c, st = s, omt = 1.0f - c;
    float kd = kx * ux + ky * uy + kz * uz;
    float sxu = ky * uz - kz * uy, syu = kz * ux - kx * uz, szu = kx * uy - ky * ux;
    float nx = ux * ct + sxu * st + kx * kd * omt;
    float ny = uy * ct + syu * st + ky * kd * omt;
    float nz = uz * ct + szu * st + kz * kd * omt;
    ux = nx; uy = ny; uz = nz;
  } else if(c < 0.0f){
    // 180-Grad-Knick: u bleibt erhalten, v zeigt danach in die Gegenrichtung
    ux = in->u[0]; uy = in->u[1]; uz = in->u[2];
  }
  // Drift korrigieren: u senkrecht auf die neue Achse, v = cross(achse,u)
  float dd = ux * ax1 + uy * ay1 + uz * az1;
  ux -= ax1 * dd; uy -= ay1 * dd; uz -= az1 * dd;
  float ul = sqrtf(ux * ux + uy * uy + uz * uz);
  if(ul < 1e-6f){ ring_basis(out, ax1, ay1, az1); return; }
  out->u[0] = ux / ul; out->u[1] = uy / ul; out->u[2] = uz / ul;
  out->v[0] = ay1 * out->u[2] - az1 * out->u[1];
  out->v[1] = az1 * out->u[0] - ax1 * out->u[2];
  out->v[2] = ax1 * out->u[1] - ay1 * out->u[0];
}

// Röhre zwischen zwei Knoten-Ringen (konisch), sides radiale Segmente.
// Schreibt 6*sides Vertices nach treeVerts + eine Wireframe-Linie.
static void emit_tube(const Ring *a, const Ring *b, int sides,
                      float sway0, float sway1,
                      float cr, float cg, float cb, float cvar){
  float px0[8][3], px1[8][3], nx[8][3];
  for(int k = 0; k < sides; k++){
    float ang = k * 6.2831853f / (float)sides;
    float ca = cosf(ang), sa = sinf(ang);
    float dx = a->u[0] * ca + a->v[0] * sa;
    float dy = a->u[1] * ca + a->v[1] * sa;
    float dz = a->u[2] * ca + a->v[2] * sa;
    px0[k][0] = a->p[0] + dx * a->r; px0[k][1] = a->p[1] + dy * a->r; px0[k][2] = a->p[2] + dz * a->r;
    nx[k][0] = dx; nx[k][1] = dy; nx[k][2] = dz;
    float ex = b->u[0] * ca + b->v[0] * sa;
    float ey = b->u[1] * ca + b->v[1] * sa;
    float ez = b->u[2] * ca + b->v[2] * sa;
    px1[k][0] = b->p[0] + ex * b->r; px1[k][1] = b->p[1] + ey * b->r; px1[k][2] = b->p[2] + ez * b->r;
  }
  for(int k = 0; k < sides; k++){
    int k2 = (k + 1) % sides;
    int idx[6]  = { k, k2, k,  k2, k2, k };
    int ring[6] = { 0,  0, 1,   0,  1, 1 };
    for(int m = 0; m < 6; m++){
      if(nTreeVerts + 1 > TREE_VERT_CAP) return;
      int kk = idx[m];
      const float *pp = ring[m] ? px1[kk] : px0[kk];
      float *d = &treeVerts[nTreeVerts * TREE_FLOATS];
      d[0] = pp[0]; d[1] = pp[1]; d[2] = pp[2];
      d[3] = nx[kk][0]; d[4] = nx[kk][1]; d[5] = nx[kk][2];
      d[6] = cr * cvar; d[7] = cg * cvar; d[8] = cb * cvar;
      d[9] = ring[m] ? sway1 : sway0;
      nTreeVerts++;
    }
  }
  add_wire(a->p[0], a->p[1], a->p[2], b->p[0], b->p[1], b->p[2]);
#ifdef TREE_SEAM_DEBUG
  // Nur im Naht-Test (/tmp/seam.py) uebersetzt: beide Ringe ausgeben, damit ein
  // Skript benachbarte Segmente (gemeinsamer Mittelpunkt) vergleichen kann.
  printf("SEG %d %.5f %.5f %.5f %.5f %.5f %.5f", sides,
         a->p[0], a->p[1], a->p[2], b->p[0], b->p[1], b->p[2]);
  for(int k = 0; k < sides; k++) printf(" %.5f %.5f %.5f", px0[k][0], px0[k][1], px0[k][2]);
  for(int k = 0; k < sides; k++) printf(" %.5f %.5f %.5f", px1[k][0], px1[k][1], px1[k][2]);
  printf("\n");
#endif
}

// Geschlossener Deckel (Dreiecksfaecher) an einem Ring — ohne ihn ist das
// Stamm-Oberteil ein offenes Loch (sichtbar von oben/in die Krone hinein).
static void emit_cap(const Ring *a, int sides, float sway,
                     float cr, float cg, float cb, float cvar){
  for(int k = 0; k < sides; k++){
    int k2 = (k + 1) % sides;
    float ang0 = k * 6.2831853f / (float)sides, ang1 = k2 * 6.2831853f / (float)sides;
    const float angs[3] = { ang0, ang1, ang0 };
    for(int m = 0; m < 3; m++){
      if(nTreeVerts + 1 > TREE_VERT_CAP) return;
      float ca = cosf(angs[m]), sa = sinf(angs[m]);
      float dx = a->u[0] * ca + a->v[0] * sa;
      float dy = a->u[1] * ca + a->v[1] * sa;
      float dz = a->u[2] * ca + a->v[2] * sa;
      float *d = &treeVerts[nTreeVerts * TREE_FLOATS];
      if(m == 2){ d[0] = a->p[0]; d[1] = a->p[1]; d[2] = a->p[2]; }
      else      { d[0] = a->p[0] + dx * a->r; d[1] = a->p[1] + dy * a->r; d[2] = a->p[2] + dz * a->r; }
      d[3] = dx * 0.3f; d[4] = 0.9f; d[5] = dz * 0.3f;
      d[6] = cr * cvar; d[7] = cg * cvar; d[8] = cb * cvar;
      d[9] = sway;
      nTreeVerts++;
    }
  }
}

// Deterministischer per-Baum-Hash (Seed aus der Weltposition)
#define TH(i) c_hash12(seed * 0.731f + (i) * 13.7f, seed * 1.173f + (i) * 7.9f)

// Einzelblatt-Instanz: Basispunkt (Welt), Groesse, Orientierung, Farbe
static void add_leaf(float x, float y, float z, float size, float rotY, float tilt,
                     float cr, float cg, float cb){
  if(nLeafInst >= LEAF_CAP) return;
  float *d = &leafInst[nLeafInst * LEAF_FLOATS];
  d[0] = x; d[1] = y; d[2] = z;
  d[3] = size; d[4] = rotY; d[5] = tilt; d[6] = 0.0f;
  d[7] = cr; d[8] = cg; d[9] = cb;
  nLeafInst++;
}
// Blattbuschel um eine Welt-Position. Die Blatter sitzen NICHT symmetrisch:
// zwei Hashes verschieben sie entlang der Astachse, damit ein Buschel wie eine
// kleine Wolke aus Einzelblaettern aussieht und nicht wie ein flacher Faecher.
static void leaf_cluster(float cx, float cy, float cz, float rad, int cnt,
                         int sb, float s, float seed){
  // TH() oben in der Datei nutzt das `seed` von gen_tree — hier ist es ein Parameter
  #define THL(i) c_hash12(seed * 0.731f + (i) * 13.7f, seed * 1.173f + (i) * 7.9f)
  for(int i = 0; i < cnt; i++){
    float h1 = THL(sb + i * 4),       h2 = THL(sb + i * 4 + 1);
    float h3 = THL(sb + i * 4 + 2),   h4 = THL(sb + i * 4 + 3);
    // gleichmaessig in einer Kugel (h1..h3) — rad ist bereits Weltmass
    float th = h1 * 6.2832f, cz2 = h2 * 2.0f - 1.0f;
    float rr = rad * cbrtf(h3);
    float sr = sqrtf(fmaxf(0.0f, 1.0f - cz2 * cz2));
    add_leaf(cx + rr * sr * cosf(th),
             cy + rr * cz2 * 0.85f + rad * 0.15f,   // Buschel sinkt leicht nach unten
             cz + rr * sr * sinf(th),
             s * (0.028f + 0.024f * h4),            // 6..13 cm bei s=2,2
             h2 * 6.2832f,
             0.25f + h3 * 1.05f,                    // Ueberhang
             0.10f + 0.16f * h1, 0.26f + 0.26f * h2, 0.05f + 0.10f * h4);
  }
  #undef THL
}

// Ein Baum in "Einheitsspace" (Höhe ~3.0), nach (bx,by,bz) verschoben, * s skaliert.
static void gen_tree(float bx, float by, float bz, float s, float seed, int near){
  // ---------- Stamm: 6 Knoten, y 0..1.5, konisch + Wurzelflan + leichte Krümmung
  float kx[6], ky[6], kz[6], kr[6];
  Ring kn[6];
  memset(kn, 0, sizeof kn);
  float curve = TH(1) * 6.283f;
  for(int k = 0; k < 6; k++){
    float y = k * 0.30f;
    float f = y / 1.5f;
    float fl = y < 0.5f ? 1.0f - y / 0.5f : 0.0f;
    kr[k] = 0.17f - 0.125f * f + 0.12f * fl * fl;
    kx[k] = 0.09f * sinf(y * 1.7f + curve) * f;
    kz[k] = 0.07f * cosf(y * 1.3f + curve * 1.7f) * f;
    ky[k] = y;
  }
  for(int k = 0; k <= 5; k++){   // alle sechs Knoten — kn[5] brauchte die Kappe
    kn[k].p[0] = bx + s * kx[k]; kn[k].p[1] = by + s * ky[k]; kn[k].p[2] = bz + s * kz[k];
    kn[k].r = s * kr[k];
  }
  // Achsen der fuenf Segmente
  float sd[5][3];
  for(int k = 0; k < 5; k++){
    float dx = kn[k+1].p[0] - kn[k].p[0], dy = kn[k+1].p[1] - kn[k].p[1], dz = kn[k+1].p[2] - kn[k].p[2];
    float l = sqrtf(dx * dx + dy * dy + dz * dz);
    if(l < 1e-5f) l = 1e-5f;
    sd[k][0] = dx / l; sd[k][1] = dy / l; sd[k][2] = dz / l;
  }
  ring_basis(&kn[0], sd[0][0], sd[0][1], sd[0][2]);
  // k = 0..4: Segment k benutzt die Ringe kn[k] und kn[k+1]. Der letzte Ring
  // (kn[5], oben fuer die Kappe) wird aus sd[4] nach sd[4] transportiert, also
  // im Grunde nur uebernommen — ohne diesen Schritt blieben u/v uninitialisiert
  // und die Kappe baute Dreiecke bis in den Wertebereich von 1e18 m.
  for(int k = 0; k < 5; k++){
    int nd = (k + 1 < 5) ? k + 1 : 4;
    ring_transport(&kn[k+1], &kn[k], sd[k][0], sd[k][1], sd[k][2], sd[nd][0], sd[nd][1], sd[nd][2]);
  }
  for(int k = 0; k < 5; k++)
    emit_tube(&kn[k], &kn[k+1], 8,
              0.05f + 0.05f * k, 0.05f + 0.05f * (k + 1),
              0.30f, 0.22f, 0.14f, 0.85f + 0.3f * TH(2 + k));
  emit_cap(&kn[5], 8, 0.30f, 0.28f, 0.21f, 0.13f, 0.9f);   // Stamm-Oberteil schliessen
  // ---------- Aeste: 5 Hauptaeste (2 Segmente) + je 2 Seitenaeste
  for(int i = 0; i < 5; i++){
    float y0 = 0.85f + 0.16f * i + (TH(10 + i) - 0.5f) * 0.12f;
    float fi = y0 / 0.30f;
    int k0 = (int)fi; if(k0 > 4) k0 = 4;
    float fr = fi - k0;
    float sx = kx[k0] + (kx[k0+1] - kx[k0]) * fr;
    float sz = kz[k0] + (kz[k0+1] - kz[k0]) * fr;
    float sr = kr[k0] + (kr[k0+1] - kr[k0]) * fr;
    float az  = i * 1.2566f + (TH(20 + i) - 0.5f) * 1.1f;      // Azimut ~72° + Jitter
    float el  = 0.5f + TH(30 + i) * 0.55f;                      // Elevation 29°..61°
    float len = 0.55f + TH(40 + i) * 0.45f;                     // Länge 0.55..1.0
    float dx = cosf(el) * cosf(az), dy = sinf(el), dz = cosf(el) * sinf(az);
    float s0x = sx + dx * sr, s0y = y0, s0z = sz + dz * sr;
    // 2. Segment: Ast biegt nach oben/außen weg
    float bend = 0.25f + TH(50 + i) * 0.35f;
    float dx2 = dx * cosf(bend), dy2 = dy + 0.5f * sinf(bend), dz2 = dz * cosf(bend);
    float dl = sqrtf(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);
    dx2 /= dl; dy2 /= dl; dz2 /= dl;
    float m0x = s0x + dx  * len * 0.55f, m0y = s0y + dy  * len * 0.55f, m0z = s0z + dz  * len * 0.55f;
    float t0x = m0x + dx2 * len * 0.45f, t0y = m0y + dy2 * len * 0.45f, t0z = m0z + dz2 * len * 0.45f;
    // Ast-Ringe: Anfangsring liegt AUF DER STAMMACHSE (nicht auf der Oberflaeche),
    // dann steckt der ganze Anfangsring im Stamm und die T-Kreuzung hat keinen
    // Spalt. Der mittlere Ring wird parallel transportiert -> kein Spalt im Ast.
    Ring br[3];
    memset(br, 0, sizeof br);
    br[0].p[0] = bx + s * sx; br[0].p[1] = by + s * y0; br[0].p[2] = bz + s * sz;
    br[0].r = s * fminf(0.075f, sr * 0.85f);   // muss in den Stamm hineinpassen
    br[1].p[0] = bx + s * m0x; br[1].p[1] = by + s * m0y; br[1].p[2] = bz + s * m0z;
    br[1].r = s * 0.03f;
    br[2].p[0] = bx + s * t0x; br[2].p[1] = by + s * t0y; br[2].p[2] = bz + s * t0z;
    br[2].r = s * 0.016f;
    ring_basis(&br[0], dx, dy, dz);
    ring_transport(&br[1], &br[0], dx, dy, dz, dx2, dy2, dz2);
    ring_transport(&br[2], &br[1], dx2, dy2, dz2, dx2, dy2, dz2);
    emit_tube(&br[0], &br[1], 6, 0.4f, 0.7f,
              0.36f, 0.28f, 0.18f, 0.85f + 0.3f * TH(55 + i));
    emit_tube(&br[1], &br[2], 6, 0.7f, 1.0f,
              0.36f, 0.28f, 0.18f, 0.85f + 0.3f * TH(56 + i));
    emit_cap(&br[2], 6, 1.0f, 0.34f, 0.27f, 0.17f, 0.9f);   // Astspitze schliessen
    // Einzelblaetter nur an nahen Baeumen (leaf.vert cullt ab 50 m ohnehin):
    // Buschel an der Astspitze und entlang des mittleren Aststuecks.
    if(near){
      leaf_cluster(bx + s * t0x, by + s * t0y, bz + s * t0z, s * 0.24f, 70, 300 + i * 200, s, seed);
      leaf_cluster(bx + s * m0x, by + s * m0y, bz + s * m0z, s * 0.20f, 30, 380 + i * 200, s, seed);
    }
    // Seitenaeste: einer am Ast-Mittel, einer an der Spitze
    for(int j = 0; j < 2; j++){
      float px = j == 0 ? m0x : t0x, py = j == 0 ? m0y : t0y, pz = j == 0 ? m0z : t0z;
      float az2 = az + (TH(60 + i * 2 + j) - 0.5f) * 2.4f + (j == 0 ? 0.7f : -0.6f);
      float el2 = el + 0.15f + TH(70 + i * 2 + j) * 0.45f;
      float len2 = 0.22f + TH(80 + i * 2 + j) * 0.22f;
      float d2x = cosf(el2) * cosf(az2), d2y = sinf(el2), d2z = cosf(el2) * sinf(az2);
      // Anfang etwas in den Mutterast zurueckziehen -> keine T-Kreuzungs-Luecke
      float back = (j == 0 ? 0.030f : 0.016f) * 0.9f;
      float t2x = px + d2x * len2, t2y = py + d2y * len2, t2z = pz + d2z * len2;
      Ring sb[2];
      memset(sb, 0, sizeof sb);
      sb[0].p[0] = bx + s * (px - d2x * back); sb[0].p[1] = by + s * (py - d2y * back); sb[0].p[2] = bz + s * (pz - d2z * back);
      sb[0].r = s * 0.02f;
      sb[1].p[0] = bx + s * t2x; sb[1].p[1] = by + s * t2y; sb[1].p[2] = bz + s * t2z;
      sb[1].r = s * 0.009f;
      ring_basis(&sb[0], d2x, d2y, d2z);
      ring_basis(&sb[1], d2x, d2y, d2z);
      emit_tube(&sb[0], &sb[1], 5, 1.0f, 1.0f,
                0.42f, 0.34f, 0.22f, 0.85f + 0.3f * TH(85 + i * 2 + j));
      // Keine Kappe an den Seitentrieb-Spitzen: 10 Kappen pro Baum wuerden
      // 150 Vertices kosten und der Baum-Puffer (TREE_VERT_CAP=700000) waere
      // voll (gemessen 743.796). Die 0,9 cm offenen Enden stecken im Blattwerk.
      if(near)
        leaf_cluster(bx + s * t2x, by + s * t2y, bz + s * t2z, s * 0.15f, 35, 600 + i * 40 + j * 20, s, seed);
    }
  }
  // ---------- Blattdach: K verteilte Billboard-Quads (prozedurale Textur + Alpha-Test) ----------
  {
    float ccx = bx, ccy = by + s * 2.05f, ccz = bz;
    float ex = s * 1.20f, ey = s * 1.00f, ez = s * 1.20f;   // Kronen-Ellipsoid
    const int K = near ? 6 : 18;   // nahe Baeume: echte Blaetter traussen, die
                                   // Quads fuellen die Innenmasse dahinter
    int variant = (int)(fabsf(seed) * 7.31f) % 4;   // Kronenform im Atlas
    static const float corners[6][2] = {
      {-0.5f,-0.5f},{0.5f,-0.5f},{-0.5f,0.5f},
      { 0.5f,-0.5f},{ 0.5f, 0.5f},{-0.5f, 0.5f}
    };
    for(int q = 0; q < K; q++){
      float h1 = TH(95 + q * 3), h2 = TH(96 + q * 3), h3 = TH(97 + q * 3);
      float h4 = TH(98 + q * 3), h5 = TH(99 + q * 3);
      // Position im Kronen-Ellipsoid (zum Zentrum gewichtet)
      float cc  = ccx + (h1 + h2 - 1.0f) * ex;
      float ccy2 = ccy + (h3 + h4 - 1.0f) * ey;
      float ccz2 = ccz + (h5 + h1 - 1.0f) * ez;
      float qsize = s * (0.95f + 0.55f * h2);   // Quad-Groesse
      float qrot  = h3 * 6.283f;                 // Rotation (Diversitaet)
      float qseed = h4 * 13.7f + q * 0.37f;
      for(int c = 0; c < 6; c++){
        if(nCrownVerts + 1 > CROWN_CAP) return;
        float *d = &crownVerts[nCrownVerts * CROWN_FLOATS];
        d[0] = cc; d[1] = ccy2; d[2] = ccz2;
        d[3] = corners[c][0]; d[4] = corners[c][1];
        d[5] = qsize; d[6] = qrot; d[7] = qseed; d[8] = (float)variant;
        nCrownVerts++;
      }
    }
    // Kronenvolumen mit echten Blaettern fuellen (nur nah): die Quads allein
    // sehen aus wie gemalte Flecken, erst die Einzelblaetter geben der Krone
    // eine echte Silhouette und Eigenbewegung.
    if(near){
      for(int i = 0; i < 700; i++){
        float h1 = TH(1200 + i * 3), h2 = TH(1201 + i * 3), h3 = TH(1202 + i * 3);
        float h4 = TH(1400 + i);
        // Punkt im Ellipsoid (h1,h2,h3 gleichmaessig in der Kugel)
        float th = h1 * 6.2832f, czz = h2 * 2.0f - 1.0f;
        float rr = cbrtf(h3);
        float sr = sqrtf(fmaxf(0.0f, 1.0f - czz * czz));
        add_leaf(ccx + rr * sr * cosf(th) * ex * 0.92f,
                 ccy + rr * czz * ey * 0.92f,
                 ccz + rr * sr * sinf(th) * ez * 0.92f,
                 s * (0.028f + 0.024f * h4), h3 * 6.2832f, 0.25f + h1 * 1.05f,
                 0.09f + 0.17f * h2, 0.24f + 0.28f * h3, 0.04f + 0.11f * h4);
      }
    }
  }
}

// ---------- Platzierung (identisch zum alten SDF-System) ----------
// Eine Zelle der Groesse G=9 m: liefert 1, wenn dort ein Baum steht, und setzt
// tp (Basispunkt) + s (Groesse). trees_build() und tree_find_nearest() nutzen
// dieselbe Funktion, damit der Debug-Sprung (Taste B) zu einem Baum gehoert, den
// es wirklich gibt.
#define TREE_G 9.0f
static int tree_cell(int ix, int iz, float *tp, float *s){
  float rx, ry;
  c_hash22(ix * 0.37f + 7.31f, iz * 0.37f + 3.17f, &rx, &ry);
  if(rx < 0.30f) return 0;                       // 70% der Zellen frei
  float px = (ix + 0.5f + (rx - 0.5f) * 0.9f) * TREE_G;
  float pz = (iz + 0.5f + (ry - 0.5f) * 0.9f) * TREE_G;
  float th = c_terrainH(px, pz);
  if(th < 0.4f || th > 6.5f) return 0;           // nur auf Wiesen
  float e = 0.6f;
  float dx = c_terrainH(px + e, pz) - th;
  float dz = c_terrainH(px, pz + e) - th;
  if(sqrtf(dx * dx + dz * dz) > 0.5f) return 0;  // keine steilen Stellen
  tp[0] = px; tp[1] = th - 0.6f; tp[2] = pz;
  *s = 1.7f + ry * 1.0f;                         // Größe 1.7..2.7
  return 1;
}

// Naechsten Baum zu (x,z) finden — Debug (Taste B). Gleiche Zellen wie oben.
int tree_find_nearest(float x, float z, float *out, float *sizeOut){
  const int RANGE = 20;
  int ix0 = (int)floorf((x - RANGE * TREE_G) / TREE_G);
  int ix1 = (int)floorf((x + RANGE * TREE_G) / TREE_G);
  int iz0 = (int)floorf((z - RANGE * TREE_G) / TREE_G);
  int iz1 = (int)floorf((z + RANGE * TREE_G) / TREE_G);
  float bd = 1e30f;
  for(int iz = iz0; iz <= iz1; iz++)
  for(int ix = ix0; ix <= ix1; ix++){
    float tp[3], s;
    if(!tree_cell(ix, iz, tp, &s)) continue;
    float dx = tp[0] - x, dz = tp[2] - z, d = dx * dx + dz * dz;
    if(d < bd){ bd = d; out[0] = tp[0]; out[1] = tp[1]; out[2] = tp[2]; if(sizeOut) *sizeOut = s; }
  }
  return bd < 1e29f;
}

// Eine Referenz-Krone aus echten Einzelblaettern (Vorlage fuer den Atlas).
// Absichtlich in BUSCHEL gebaut — 26 Klumpen a ~22 Blaetter — weil eine
// gleichmaessige Fuellung aus der Ferne wie ein gruerner Nebel aussieht,
// Haufen dagegen wie Blattwerk mit Luecken dazwischen.
int foliage_patch(float *out, int cap, float seed, float s){
  #define THP(i) c_hash12(seed * 0.731f + (i) * 13.7f, seed * 1.173f + (i) * 7.9f)
  int n = 0;
  float ccy = 2.05f * s, ex = 1.20f * s, ey = 1.00f * s, ez = 1.20f * s;
  for(int c = 0; c < 40 && n < cap; c++){
    float g1 = THP(c * 3), g2 = THP(c * 3 + 1), g3 = THP(c * 3 + 2);
    float bx = (g1 + g2 - 1.0f) * ex * 0.82f;
    float by = ccy + (g3 + g1 - 1.0f) * ey * 0.82f;
    float bz = (g2 + g3 - 1.0f) * ez * 0.82f;
    float rad = s * (0.20f + 0.16f * g2);
    for(int i = 0; i < 50 && n < cap; i++){
      int k = c * 32 + i;
      float h1 = THP(400 + k * 3), h2 = THP(401 + k * 3), h3 = THP(402 + k * 3);
      float th = h1 * 6.2832f, cz2 = h2 * 2.0f - 1.0f;
      float rr = rad * cbrtf(h3);
      float sr = sqrtf(fmaxf(0.0f, 1.0f - cz2 * cz2));
      float *d = &out[n * LEAF_FLOATS];
      d[0] = bx + rr * sr * cosf(th);
      d[1] = by + rr * cz2 * 0.85f;
      d[2] = bz + rr * sr * sinf(th);
      d[3] = s * (0.120f + 0.070f * h1);      // Atlas-Blaetter sind bewusst gross:
      d[4] = h2 * 6.2832f;                    // sie stehen fuer einen Blattbuschel,
      d[5] = 0.25f + h3 * 1.05f;              // nicht fuer ein Einzelblatt
      d[6] = 0.0f;
      d[7] = 0.09f + 0.17f * h2; d[8] = 0.24f + 0.28f * h3; d[9] = 0.04f + 0.11f * h1;
      n++;
    }
  }
  #undef THP
  return n;
}

void trees_build(float tx, float tz){
  nTreeVerts = 0; nCrownVerts = 0; nWireVerts = 0; nTrees = 0; nLeafInst = 0;
  const int RANGE = 20;   // Zellen-Radius um das Kameraziel (~180m, passt zur
                          // groesseren Sichtweite: march bis 400m)
  int ix0 = (int)floorf((tx - RANGE * TREE_G) / TREE_G);
  int ix1 = (int)floorf((tx + RANGE * TREE_G) / TREE_G);
  int iz0 = (int)floorf((tz - RANGE * TREE_G) / TREE_G);
  int iz1 = (int)floorf((tz + RANGE * TREE_G) / TREE_G);
  for(int iz = iz0; iz <= iz1; iz++)
  for(int ix = ix0; ix <= ix1; ix++){
    float tp[3], s;
    if(!tree_cell(ix, iz, tp, &s)) continue;
    // Nahe Baeume bekommen echte Einzelblaetter, ferne nur die Billboards.
    // 50 m = die Grenze, ab der leaf.vert die Blaeue selbst auscullt.
    float ddx = tp[0] - tx, ddz = tp[2] - tz;
    gen_tree(tp[0], tp[1], tp[2], s, tp[0] * 12.9898f + tp[2] * 78.233f,
             ddx * ddx + ddz * ddz < 2500.0f);
    nTrees++;
  }
}
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
float wireVerts[WIRE_CAP * WIRE_FLOATS];
int nTreeVerts = 0, nCrownVerts = 0, nWireVerts = 0, nTrees = 0;

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

// Ein Baum in "Einheitsspace" (Höhe ~3.0), nach (bx,by,bz) verschoben, * s skaliert.
static void gen_tree(float bx, float by, float bz, float s, float seed){
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
    }
  }
  // ---------- Blattdach: K verteilte Billboard-Quads (prozedurale Textur + Alpha-Test) ----------
  {
    float ccx = bx, ccy = by + s * 2.05f, ccz = bz;
    float ex = s * 1.20f, ey = s * 1.00f, ez = s * 1.20f;   // Kronen-Ellipsoid
    const int K = 18;
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
        d[5] = qsize; d[6] = qrot; d[7] = qseed; d[8] = 0.0f;
        nCrownVerts++;
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

void trees_build(float tx, float tz){
  nTreeVerts = 0; nCrownVerts = 0; nWireVerts = 0; nTrees = 0;
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
    gen_tree(tp[0], tp[1], tp[2], s, tp[0] * 12.9898f + tp[2] * 78.233f);
    nTrees++;
  }
}
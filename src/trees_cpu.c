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
                      float cr, float cg, float cb, float cvar, float v0){
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
  float vdx = b->p[0] - a->p[0], vdy = b->p[1] - a->p[1], vdz = b->p[2] - a->p[2];
  float v1 = v0 + sqrtf(vdx * vdx + vdy * vdy + vdz * vdz);
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
      d[10] = (float)kk / (float)sides;          // u: einmal rund um das Rohr
      d[11] = ring[m] ? v1 : v0;                 // v: Bogenlaenge entlang des Rohrs
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
                     float cr, float cg, float cb, float cvar, float v0){
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
      d[10] = angs[m] / 6.2831853f;
      d[11] = v0;
      nTreeVerts++;
    }
  }
}

// Deterministischer per-Baum-Hash (Seed aus der Weltposition)
#define TH(i) c_hash12(seed * 0.731f + (i) * 13.7f, seed * 1.173f + (i) * 7.9f)

// Einzelblatt-Instanz: Basispunkt (Welt), Groesse, Orientierung, Seitenverhaeltnis, Farbe
static void add_leaf(float x, float y, float z, float size, float rotY, float tilt,
                     float aspect, float cr, float cg, float cb){
  if(nLeafInst >= LEAF_CAP) return;
  float *d = &leafInst[nLeafInst * LEAF_FLOATS];
  d[0] = x; d[1] = y; d[2] = z;
  d[3] = size; d[4] = rotY; d[5] = tilt; d[6] = aspect;   // aspect = Breite/Laenge
  d[7] = cr; d[8] = cg; d[9] = cb;
  nLeafInst++;
}
// Ein Baum in "Einheitsspace" (Höhe ~3.0), nach (bx,by,bz) verschoben, * s skaliert.
// Baum-Hierarchie nach Sapling-Vorbild: Stamm -> Aeste -> Zweige. Die Blaetter
// sitzen AUF den Zweigen (bzw. Aesten) und nicht in einem Zufallsvolumen, und sie
// stehen im goldenen Winkel (137,5°) versetzt — das ist die billige Version von
// "kein Blatt abschirmen das naechste".
//   tier 3 = sehr nah : Zweige als eigene Rohre + vollstaendige Blaetter
//   tier 2 = nah      : Blaetter direkt an den Aesten, wenige Fuell-Quads
//   tier 1 = mittel   : weniger Blaetter + mehr Fuell-Quads
//   tier 0 = fern     : nur Atlas-Quads
#define GOLD 2.39996f          // 137,5 Grad

// Kronenform = Profil der Astlaengen entlang des Stamms (Sapling macht genau so
// den Unterschied zwischen Birke, Ahorn und Trauerweide).
static float crownProf(int shape, float f){
  switch(shape){
    case 0:  return 0.15f + 0.85f * f;                                   // konisch
    case 1:  return 0.20f + 0.80f * sinf(3.14159f * f);                  // rund
    case 2:  return f <= 0.7f ? 0.5f + 0.5f * f / 0.7f
                              : 0.5f + 0.5f * (1.0f - f) / 0.3f;         // breite Mitte
    case 3:  return 1.0f - 0.75f * f;                                    // Trauerform
    default: return 0.55f + 0.45f * (1.0f - f);                          // flache Oberkrone
  }
}

static void hsv2rgb(float h, float s, float v, float *r, float *g, float *b){
  float ii = h * 6.0f, fr = ii - floorf(ii);
  float p = v * (1.0f - s), q = v * (1.0f - s * fr), t = v * (1.0f - s * (1.0f - fr));
  switch((int)ii % 6){
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default:*r = v; *g = p; *b = q; break;
  }
}

// Blaetter entlang eines geraden Aests/Zweigs. dir = Achse, (u,v) = mitgefuehrte
// Querbasis (parallel transportiert, damit die Blaetter nicht an der Achse
// "umklappen"), cnt = Anzahl, size = Blattlaenge in Metern, aspect = Breite/Laenge.
static void leaves_along(const float ax[3], const float dir[3],
                         const float u[3], const float v[3],
                         float len, float rad, int cnt, float size, float aspect,
                         float tiltBase, float seed, int sb,
                         float lcr, float lcgg, float lcb){
  for(int i = 0; i < cnt; i++){
    if(nLeafInst >= LEAF_CAP) return;
    float h1 = c_hash12(seed * 0.731f + (sb + i) * 13.7f, seed * 1.173f + (sb + i) * 7.9f);
    float h2 = c_hash12(seed * 0.731f + (sb + i) * 17.3f, seed * 1.173f + (sb + i) * 11.1f);
    float uu = (i + 0.5f) / (float)cnt;
    float ph = i * GOLD + sb * 0.37f;
    float ca = cosf(ph), sa = sinf(ph);
    float rx = u[0] * ca + v[0] * sa, ry = u[1] * ca + v[1] * sa, rz = u[2] * ca + v[2] * sa;
    // Sapling: leafDownAngle ~45° gegenueber dem Mutterast, zur Spitze flacher
    float da = tiltBase - 0.35f * uu + (h1 - 0.5f) * 0.55f;
    float cd = cosf(da), sd = sinf(da);
    float lx = dir[0] * cd + rx * sd, ly = dir[1] * cd + ry * sd - 0.22f, lz = dir[2] * cd + rz * sd;
    float ll = sqrtf(lx * lx + ly * ly + lz * lz); if(ll < 1e-6f) ll = 1e-6f;
    lx /= ll; ly /= ll; lz /= ll;
    // leaf.vert baut die Blattlaangsachse als (cos(tilt)*cos(rotY), -sin(tilt),
    // -cos(tilt)*sin(rotY)) -> aus der Zielrichtung zurueckgerechnet:
    float tilt = -asinf(fmaxf(-1.0f, fminf(1.0f, ly)));
    float ct = cosf(tilt); if(ct < 1e-3f) ct = 1e-3f;
    float rotY = atan2f(-lz, lx);
    float sz = size * (0.75f + 0.45f * h2) * (1.0f - 0.30f * uu);   // leafScaleT
    add_leaf(ax[0] + dir[0] * len * uu + rx * rad,
             ax[1] + dir[1] * len * uu + ry * rad,
             ax[2] + dir[2] * len * uu + rz * rad,
             sz, rotY, tilt, aspect, lcr, lcgg, lcb);
  }
}

static void gen_tree(float bx, float by, float bz, float s, float seed, int tier){
  // ---------- jeder Baum ein Individuum ----------
  float hf    = 0.80f + TH(1) * 2.20f;          // Groesse 0,8..3,0
  int   nb1   = 5 + (int)(TH(2) * 5.0f);        // 5..9 Aeste
  float az0   = TH(3) * 6.2832f;
  float elB   = 0.40f + TH(4) * 0.40f;          // Ast-Elevation
  float bendB = 0.18f + TH(5) * 0.50f;          // Astbiegung
  int   shape = (int)(TH(6) * 4.99f);           // Kronenform
  int   nTwig = 6 + (int)(TH(7) * 11.0f);       // Zweige pro Ast (nur tier 3)
  float hue   = 0.20f + TH(8) * 0.20f;          // Blattfarbe individuell
  float lsat  = 0.45f + TH(9) * 0.45f;
  float lval  = 0.30f + TH(10) * 0.30f;
  float lsize = 0.055f + TH(11) * 0.060f;       // 5,5..11,5 cm
  float lasp  = 0.35f + TH(12) * 0.30f;         // Breite/Laenge (Sapling: nie 1)
  float tiltB = 0.55f + TH(13) * 0.55f;         // Blattneigung gegenueber dem Ast
  float bTone = 0.80f + TH(14) * 0.45f;         // Rindenton
  float H     = 1.5f * hf;                      // Stammhoehe in s-Einheiten
  float lcr, lcgg, lcb;
  hsv2rgb(hue, lsat, lval, &lcr, &lcgg, &lcb);

  // ---------- Stamm: 6 Knoten, konisch + Wurzelflan + individueller Schwung ----------
  float kx[6], ky[6], kz[6], kr[6];
  Ring kn[6];
  memset(kn, 0, sizeof kn);
  float curve = TH(15) * 6.2832f;
  float leanx = (TH(16) - 0.5f) * 0.30f, leanz = (TH(17) - 0.5f) * 0.30f;
  for(int k = 0; k < 6; k++){
    float y = k * H / 5.0f, f = y / H;
    float fl = y < 0.5f * hf ? 1.0f - y / (0.5f * hf) : 0.0f;
    kr[k] = (0.17f - 0.125f * f) * (0.75f + 0.45f * hf) + 0.12f * fl * fl;
    kx[k] = 0.09f * sinf(y * 1.7f + curve) * f + leanx * f * f;
    kz[k] = 0.07f * cosf(y * 1.3f + curve * 1.7f) * f + leanz * f * f;
    ky[k] = y;
  }
  for(int k = 0; k <= 5; k++){
    kn[k].p[0] = bx + s * kx[k]; kn[k].p[1] = by + s * ky[k]; kn[k].p[2] = bz + s * kz[k];
    kn[k].r = s * kr[k];
  }
  float sd[5][3];
  for(int k = 0; k < 5; k++){
    float dx = kn[k+1].p[0] - kn[k].p[0], dy = kn[k+1].p[1] - kn[k].p[1], dz = kn[k+1].p[2] - kn[k].p[2];
    float l = sqrtf(dx * dx + dy * dy + dz * dz);
    if(l < 1e-5f) l = 1e-5f;
    sd[k][0] = dx / l; sd[k][1] = dy / l; sd[k][2] = dz / l;
  }
  ring_basis(&kn[0], sd[0][0], sd[0][1], sd[0][2]);
  for(int k = 0; k < 5; k++){
    int nd = (k + 1 < 5) ? k + 1 : 4;
    ring_transport(&kn[k+1], &kn[k], sd[k][0], sd[k][1], sd[k][2], sd[nd][0], sd[nd][1], sd[nd][2]);
  }
  // v = Bogenlaenge entlang des Stamms (Rindenmuster laeuft mit dem Stamm hoch)
  float vlen = 0.0f;
  for(int k = 0; k < 5; k++){
    emit_tube(&kn[k], &kn[k+1], 8,
              0.05f + 0.05f * k, 0.05f + 0.05f * (k + 1),
              0.30f * bTone, 0.22f * bTone, 0.14f * bTone, 0.85f + 0.3f * TH(2 + k), vlen);
    vlen += s * H / 5.0f;
  }
  emit_cap(&kn[5], 8, 0.30f, 0.28f * bTone, 0.21f * bTone, 0.13f * bTone, 0.9f, vlen);

  // ---------- Aeste: nb1 Stueck, goldener Winkel, Laenge nach Kronenprofil ----------
  float tipx[16], tipy[16], tipz[16]; int nTip = 0;
  for(int i = 0; i < nb1; i++){
    float fr = (i + 0.5f + (TH(20 + i) - 0.5f) * 0.6f) / (float)nb1;
    fr = fmaxf(0.0f, fminf(1.0f, fr));
    float y0 = H * (0.30f + 0.68f * fr);
    float fi = y0 / (H / 5.0f);
    int k0 = (int)fi; if(k0 > 4) k0 = 4;
    float frc = fi - k0;
    float sx = kx[k0] + (kx[k0+1] - kx[k0]) * frc;
    float sz = kz[k0] + (kz[k0+1] - kz[k0]) * frc;
    float sr = kr[k0] + (kr[k0+1] - kr[k0]) * frc;
    float az  = az0 + i * GOLD + (TH(30 + i) - 0.5f) * 0.7f;
    float el  = elB + (TH(35 + i) - 0.5f) * 0.5f - (shape == 3 ? 0.30f * fr : 0.0f);
    float len = (0.45f + 0.55f * TH(40 + i)) * crownProf(shape, fr) * hf;
    float dx = cosf(el) * cosf(az), dy = sinf(el), dz = cosf(el) * sinf(az);
    float s0x = sx + dx * sr, s0y = y0, s0z = sz + dz * sr;
    float bend = bendB + TH(50 + i) * 0.35f - (shape == 3 ? 0.55f : 0.0f);
    float dx2 = dx * cosf(bend), dy2 = dy + 0.5f * sinf(bend), dz2 = dz * cosf(bend);
    float dl = sqrtf(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);
    dx2 /= dl; dy2 /= dl; dz2 /= dl;
    float m0x = s0x + dx  * len * 0.55f, m0y = s0y + dy  * len * 0.55f, m0z = s0z + dz  * len * 0.55f;
    float t0x = m0x + dx2 * len * 0.45f, t0y = m0y + dy2 * len * 0.45f, t0z = m0z + dz2 * len * 0.45f;
    Ring br[3];
    memset(br, 0, sizeof br);
    br[0].p[0] = bx + s * sx; br[0].p[1] = by + s * y0; br[0].p[2] = bz + s * sz;
    br[0].r = s * fminf(0.075f * hf, sr * 0.85f);
    br[1].p[0] = bx + s * m0x; br[1].p[1] = by + s * m0y; br[1].p[2] = bz + s * m0z;
    br[1].r = s * 0.030f * hf;
    br[2].p[0] = bx + s * t0x; br[2].p[1] = by + s * t0y; br[2].p[2] = bz + s * t0z;
    br[2].r = s * 0.016f * hf;
    ring_basis(&br[0], dx, dy, dz);
    ring_transport(&br[1], &br[0], dx, dy, dz, dx2, dy2, dz2);
    ring_transport(&br[2], &br[1], dx2, dy2, dz2, dx2, dy2, dz2);
    float bv = 0.0f;
    emit_tube(&br[0], &br[1], 6, 0.4f, 0.7f, 0.36f * bTone, 0.28f * bTone, 0.18f * bTone, 0.85f + 0.3f * TH(55 + i), bv);
    bv += s * len * 0.55f;
    emit_tube(&br[1], &br[2], 6, 0.7f, 1.0f, 0.36f * bTone, 0.28f * bTone, 0.18f * bTone, 0.85f + 0.3f * TH(56 + i), bv);
    bv += s * len * 0.45f;
    emit_cap(&br[2], 6, 1.0f, 0.34f * bTone, 0.27f * bTone, 0.17f * bTone, 0.9f, bv);
    if(nTip < 16){ tipx[nTip] = bx + s * t0x; tipy[nTip] = by + s * t0y; tipz[nTip] = bz + s * t0z; nTip++; }

    if(tier >= 3){
      // Zweige: eigene kurze Rohre entlang des Aests, darauf die Blaetter.
      for(int j = 0; j < nTwig; j++){
        float tj = 0.30f + 0.70f * (j + 0.5f) / (float)nTwig;
        float px, py, pz;
        Ring tw[2];
        int jj = i * 32 + j;
        if(tj < 0.55f){
          float w = tj / 0.55f;
          px = s0x + (m0x - s0x) * w; py = s0y + (m0y - s0y) * w; pz = s0z + (m0z - s0z) * w;
        } else {
          float w = (tj - 0.55f) / 0.45f;
          px = m0x + (t0x - m0x) * w; py = m0y + (t0y - m0y) * w; pz = m0z + (t0z - m0z) * w;
        }
        float az3 = az0 + jj * GOLD + (TH(200 + jj) - 0.5f) * 0.8f;
        float el3 = el - 0.20f + TH(210 + jj) * 0.55f - (shape == 3 ? 0.45f : 0.0f);
        float d3x = cosf(el3) * cosf(az3), d3y = sinf(el3), d3z = cosf(el3) * sinf(az3);
        float len3 = len * (0.22f + 0.22f * TH(220 + jj));
        float back = br[1].r / s * 0.9f;
        memset(tw, 0, sizeof tw);
        tw[0].p[0] = bx + s * (px - d3x * back); tw[0].p[1] = by + s * (py - d3y * back); tw[0].p[2] = bz + s * (pz - d3z * back);
        tw[0].r = s * 0.012f * hf;
        tw[1].p[0] = bx + s * (px + d3x * len3); tw[1].p[1] = by + s * (py + d3y * len3); tw[1].p[2] = bz + s * (pz + d3z * len3);
        tw[1].r = s * 0.006f * hf;
        ring_basis(&tw[0], d3x, d3y, d3z);
        ring_transport(&tw[1], &tw[0], d3x, d3y, d3z, d3x, d3y, d3z);
        emit_tube(&tw[0], &tw[1], 4, 1.0f, 1.0f, 0.40f * bTone, 0.32f * bTone, 0.20f * bTone, 0.9f, 0.0f);
        float st[3] = { tw[0].p[0], tw[0].p[1], tw[0].p[2] };
        float dr[3] = { d3x, d3y, d3z };
        leaves_along(st, dr, tw[0].u, tw[0].v, s * len3, s * 0.012f * hf,
                     700, lsize, lasp, tiltB, seed, 3000 + jj * 1000, lcr, lcgg, lcb);
      }
    } else if(tier == 2){
      // Blaetter direkt an den beiden Astsegmenten (ohne eigene Zweige)
      int per = 1000;
      float st1[3] = { br[0].p[0], br[0].p[1], br[0].p[2] };
      float d1[3]  = { dx, dy, dz };
      float st2[3] = { br[1].p[0], br[1].p[1], br[1].p[2] };
      float d2[3]  = { dx2, dy2, dz2 };
      leaves_along(st1, d1, br[0].u, br[0].v, s * len * 0.55f, br[0].r, per,
                   lsize, lasp, tiltB, seed, 300 + i * 4000, lcr, lcgg, lcb);
      leaves_along(st2, d2, br[1].u, br[1].v, s * len * 0.45f, br[1].r, per,
                   lsize, lasp, tiltB, seed, 2300 + i * 4000, lcr, lcgg, lcb);
    }
  }

  // ---------- Kronen-Quads: sitzen um die echten Astspitzen ----------
  {
    int K = 12;
    int variant = (int)(fabsf(seed) * 7.31f) % 4;
    float ccy = by + s * H * 1.15f;
    static const float corners[6][2] = {
      {-0.5f,-0.5f},{0.5f,-0.5f},{-0.5f,0.5f},
      { 0.5f,-0.5f},{ 0.5f, 0.5f},{-0.5f, 0.5f}
    };
    for(int q = 0; q < K; q++){
      float h1 = TH(95 + q * 3), h2 = TH(96 + q * 3), h3 = TH(97 + q * 3);
      float h4 = TH(98 + q * 3), h5 = TH(99 + q * 3);
      float cc, ccy2, ccz2;
      if(nTip > 0 && q % 3 != 2){          // 2/3 der Quads an den Astspitzen
        int ti = (q / 3) % nTip;
        cc   = tipx[ti] + (h1 - 0.5f) * s * 0.7f * hf;
        ccy2 = tipy[ti] + (h3 - 0.5f) * s * 0.6f * hf;
        ccz2 = tipz[ti] + (h5 - 0.5f) * s * 0.7f * hf;
      } else {                             // Rest um die Kronenmitte
        cc   = bx + (h1 + h2 - 1.0f) * s * 0.95f * hf;
        ccy2 = ccy + (h3 + h4 - 1.0f) * s * 0.80f * hf;
        ccz2 = bz + (h5 + h1 - 1.0f) * s * 0.95f * hf;
      }
      float qsize = s * (0.85f + 0.55f * h2) * (0.7f + 0.45f * hf);
      float qrot  = h3 * 6.283f;
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
  }
}
// ---------- Platzierung (identisch zum alten SDF-System) ----------
// Eine Zelle der Groesse G=9 m: liefert 1, wenn dort ein Baum steht, und setzt
// tp (Basispunkt) + s (Groesse). trees_build() und tree_find_nearest() nutzen
// dieselbe Funktion, damit der Debug-Sprung (Taste B) zu einem Baum gehoert, den
// es wirklich gibt.
#define TREE_G 9.0f
static int tree_cell(int ix, int iz, float *tp, float *s, float *hfOut){
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
  // Groessenfaktor, den gen_tree aus demselben Seed holt — fuer die Kamerarahmung
  // (die Krone sitzt bei by + s*1.5*hf*1.15).
  if(hfOut){
    float sd = px * 12.9898f + pz * 78.233f;
    *hfOut = 0.80f + c_hash12(sd * 0.123f + 1.111f, sd * 0.456f + 2.222f) * 2.20f;
  }
  return 1;
}

// Naechsten Baum zu (x,z) finden — Debug (Taste B). Gleiche Zellen wie oben.
int tree_find_nearest(float x, float z, float *out, float *sizeOut, float *hfOut){
  const int RANGE = 20;
  int ix0 = (int)floorf((x - RANGE * TREE_G) / TREE_G);
  int ix1 = (int)floorf((x + RANGE * TREE_G) / TREE_G);
  int iz0 = (int)floorf((z - RANGE * TREE_G) / TREE_G);
  int iz1 = (int)floorf((z + RANGE * TREE_G) / TREE_G);
  float bd = 1e30f;
  for(int iz = iz0; iz <= iz1; iz++)
  for(int ix = ix0; ix <= ix1; ix++){
    float tp[3], s, hf;
    if(!tree_cell(ix, iz, tp, &s, &hf)) continue;
    float dx = tp[0] - x, dz = tp[2] - z, d = dx * dx + dz * dz;
    if(d < bd){
      bd = d; out[0] = tp[0]; out[1] = tp[1]; out[2] = tp[2];
      if(sizeOut) *sizeOut = s;
      if(hfOut)   *hfOut = hf;
    }
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

typedef struct { float p[3]; float s; float d2; } TreeCand;

void trees_build(float tx, float tz){
  nTreeVerts = 0; nCrownVerts = 0; nWireVerts = 0; nTrees = 0; nLeafInst = 0;
  #define CAND_CAP 4096
  static TreeCand cand[CAND_CAP];
  int nc = 0;
  const int RANGE = 20;   // Zellen-Radius um das Kameraziel (~180m, passt zur
                          // groesseren Sichtweite: march bis 400m)
  int ix0 = (int)floorf((tx - RANGE * TREE_G) / TREE_G);
  int ix1 = (int)floorf((tx + RANGE * TREE_G) / TREE_G);
  int iz0 = (int)floorf((tz - RANGE * TREE_G) / TREE_G);
  int iz1 = (int)floorf((tz + RANGE * TREE_G) / TREE_G);
  for(int iz = iz0; iz <= iz1; iz++)
  for(int ix = ix0; ix <= ix1; ix++){
    float tp[3], s;
    if(!tree_cell(ix, iz, tp, &s, 0)) continue;
    if(nc < CAND_CAP){
      cand[nc].p[0] = tp[0]; cand[nc].p[1] = tp[1]; cand[nc].p[2] = tp[2];
      cand[nc].s = s;
      float ddx = tp[0] - tx, ddz = tp[2] - tz;
      cand[nc].d2 = ddx * ddx + ddz * ddz;
      nc++;
    }
  }
  // NAHE vor FERN erzeugen: das Blatt-Budget (LEAF_CAP) ist begrenzt. In
  // Zell-Reihenfolge hatten ferne Baeume es schon aufgebraucht, bevor der
  // Baum direkt vor der Kamera dran war (gemessen: 300k Instanzen verpufft).
  for(int i = 0; i < nc; i++)
    for(int j = i + 1; j < nc; j++)
      if(cand[j].d2 < cand[i].d2){ TreeCand t = cand[i]; cand[i] = cand[j]; cand[j] = t; }
  for(int i = 0; i < nc; i++){
    // Detail-Stufe nach Entfernung zum Kameraziel: sehr nahe Baeume bekommen
    // Zweige + vollstaendige Blaetter (bis ~100.000), mittlere nur Blaetter an
    // den Aesten, ferne nur die Atlas-Quads.
    int tier = cand[i].d2 < 121.0f ? 3 : (cand[i].d2 < 784.0f ? 2 : 0);
    gen_tree(cand[i].p[0], cand[i].p[1], cand[i].p[2], cand[i].s,
             cand[i].p[0] * 12.9898f + cand[i].p[2] * 78.233f, tier);
    nTrees++;
  }
}
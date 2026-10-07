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

// Röhre zwischen zwei Punkten (konisch), sides radiale Segmente.
// Schreibt 6*sides Vertices nach treeVerts + eine Wireframe-Linie.
static void emit_tube(float x0, float y0, float z0, float x1, float y1, float z1,
                      float r0, float r1, int sides, float sway0, float sway1,
                      float cr, float cg, float cb, float cvar){
  float ax = x1 - x0, ay = y1 - y0, az = z1 - z0;
  float al = sqrtf(ax * ax + ay * ay + az * az);
  if(al < 1e-5f) return;
  ax /= al; ay /= al; az /= al;
  // orthogonale Basis zur Achse
  float ux, uy, uz;
  if(fabsf(ay) < 0.9f){ ux = -az; uy = 0.0f; uz = ax; }   // cross(axis,(0,1,0))
  else                { ux = 0.0f; uy = az; uz = -ay; }   // cross(axis,(1,0,0))
  float ul = sqrtf(ux * ux + uy * uy + uz * uz);
  if(ul < 1e-6f) return;
  ux /= ul; uy /= ul; uz /= ul;
  float vx = ay * uz - az * uy;
  float vy = az * ux - ax * uz;
  float vz = ax * uy - ay * ux;
  float px0[8][3], px1[8][3], nx[8][3];
  for(int k = 0; k < sides; k++){
    float a = k * 6.2831853f / (float)sides;
    float dx = ux * cosf(a) + vx * sinf(a);
    float dy = uy * cosf(a) + vy * sinf(a);
    float dz = uz * cosf(a) + vz * sinf(a);
    px0[k][0] = x0 + dx * r0; px0[k][1] = y0 + dy * r0; px0[k][2] = z0 + dz * r0;
    px1[k][0] = x1 + dx * r1; px1[k][1] = y1 + dy * r1; px1[k][2] = z1 + dz * r1;
    nx[k][0] = dx; nx[k][1] = dy; nx[k][2] = dz;
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
  add_wire(x0, y0, z0, x1, y1, z1);
}

// Deterministischer per-Baum-Hash (Seed aus der Weltposition)
#define TH(i) c_hash12(seed * 0.731f + (i) * 13.7f, seed * 1.173f + (i) * 7.9f)

// Ein Baum in "Einheitsspace" (Höhe ~3.0), nach (bx,by,bz) verschoben, * s skaliert.
static void gen_tree(float bx, float by, float bz, float s, float seed){
  // ---------- Stamm: 6 Knoten, y 0..1.5, konisch + Wurzelflan + leichte Krümmung
  float kx[6], ky[6], kz[6], kr[6];
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
  for(int k = 0; k < 5; k++){
    emit_tube(bx + s * kx[k],   by + s * ky[k],   bz + s * kz[k],
              bx + s * kx[k+1], by + s * ky[k+1], bz + s * kz[k+1],
              s * kr[k], s * kr[k+1], 8,
              0.05f + 0.05f * k, 0.05f + 0.05f * (k + 1),
              0.30f, 0.22f, 0.14f, 0.85f + 0.3f * TH(2 + k));
  }
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
    emit_tube(bx + s * s0x, by + s * s0y, bz + s * s0z,
              bx + s * m0x, by + s * m0y, bz + s * m0z,
              s * 0.055f, s * 0.03f, 6, 0.4f, 0.7f,
              0.36f, 0.28f, 0.18f, 0.85f + 0.3f * TH(55 + i));
    emit_tube(bx + s * m0x, by + s * m0y, bz + s * m0z,
              bx + s * t0x, by + s * t0y, bz + s * t0z,
              s * 0.03f, s * 0.016f, 6, 0.7f, 1.0f,
              0.36f, 0.28f, 0.18f, 0.85f + 0.3f * TH(56 + i));
    // Seitenaeste: einer am Ast-Mittel, einer an der Spitze
    for(int j = 0; j < 2; j++){
      float px = j == 0 ? m0x : t0x, py = j == 0 ? m0y : t0y, pz = j == 0 ? m0z : t0z;
      float az2 = az + (TH(60 + i * 2 + j) - 0.5f) * 2.4f + (j == 0 ? 0.7f : -0.6f);
      float el2 = el + 0.15f + TH(70 + i * 2 + j) * 0.45f;
      float len2 = 0.22f + TH(80 + i * 2 + j) * 0.22f;
      float d2x = cosf(el2) * cosf(az2), d2y = sinf(el2), d2z = cosf(el2) * sinf(az2);
      float t2x = px + d2x * len2, t2y = py + d2y * len2, t2z = pz + d2z * len2;
      emit_tube(bx + s * px, by + s * py, bz + s * pz,
                bx + s * t2x, by + s * t2y, bz + s * t2z,
                s * 0.02f, s * 0.009f, 5, 1.0f, 1.0f,
                0.42f, 0.34f, 0.22f, 0.85f + 0.3f * TH(85 + i * 2 + j));
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
void trees_build(float tx, float tz){
  nTreeVerts = 0; nCrownVerts = 0; nWireVerts = 0; nTrees = 0;
  const float G = 9.0f;
  const int RANGE = 20;   // Zellen-Radius um das Kameraziel (~180m, passt zur
                          // groesseren Sichtweite: march bis 400m)
  int ix0 = (int)floorf((tx - RANGE * G) / G);
  int ix1 = (int)floorf((tx + RANGE * G) / G);
  int iz0 = (int)floorf((tz - RANGE * G) / G);
  int iz1 = (int)floorf((tz + RANGE * G) / G);
  for(int iz = iz0; iz <= iz1; iz++)
  for(int ix = ix0; ix <= ix1; ix++){
    float rx, ry;
    c_hash22(ix * 0.37f + 7.31f, iz * 0.37f + 3.17f, &rx, &ry);
    if(rx < 0.30f) continue;                       // 70% der Zellen frei
    float tp_x = (ix + 0.5f + (rx - 0.5f) * 0.9f) * G;
    float tp_z = (iz + 0.5f + (ry - 0.5f) * 0.9f) * G;
    float th = c_terrainH(tp_x, tp_z);
    if(th < 0.4f || th > 6.5f) continue;           // nur auf Wiesen
    float e = 0.6f;
    float dx = c_terrainH(tp_x + e, tp_z) - th;
    float dz = c_terrainH(tp_x, tp_z + e) - th;
    if(sqrtf(dx * dx + dz * dz) > 0.5f) continue;  // keine steilen Stellen
    float s = 1.7f + ry * 1.0f;                    // Größe 1.7..2.7
    gen_tree(tp_x, th - 0.6f, tp_z, s, tp_x * 12.9898f + tp_z * 78.233f);
    nTrees++;
  }
}
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

int   treeOneShot = 0;
int   treeGenMode = 1;   // 0 = eigener Generator, 1 = EZ-Tree-Port
int   treeEzPreset = -1;   // DEMO_EZPRESET=0..3: Preset erzwingen (-1 = pro Baum gewuerfelt)
float treeEzSpread = 1.0f;  // DEMO_EZSPREAD: Laengenfaktor der Kronenlevel (1 = 1:1-Port)
const char *ezDbgName = "-"; float ezDbgUnit = 0.0f, ezDbgR0 = 0.0f;
float ezDbgR[13]; int ezDbgNSec = 0;
float ezDbgS = 0.0f, ezDbgHf = 0.0f;
int   treeForceTier = -1;
float treeVerts[TREE_VERT_CAP * TREE_FLOATS];float crownVerts[CROWN_CAP * CROWN_FLOATS];
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

// Astspitzen + Laubpositionen des aktuellen Baums ( fuer die Distanz-Quads).
// Steht hier oben, weil sowohl der alte als auch der EZ-Generator sie fuellen.
#define EZ_TIPCAP 48
static float ezTipx[EZ_TIPCAP], ezTipy[EZ_TIPCAP], ezTipz[EZ_TIPCAP];
static int   ezNTip = 0;
#define EZ_SPRIGCAP 512
static float ezSx[EZ_SPRIGCAP], ezSy[EZ_SPRIGCAP], ezSz[EZ_SPRIGCAP], ezSsize[EZ_SPRIGCAP];
static int   ezNSprig = 0;

// Kronen-Quads (Distanz-LOD): 2/3 an den echten Astspitzen, Rest um die
// Kronenmitte. Steht beiden Generatoren zur Verfuegung.
static void emit_crown_quads(float bx, float bz, float s, float hf,
                             float seed, float crownY,
                             const float *tipx, const float *tipy, const float *tipz, int nTip){
  int K = 12;
  int variant = (int)(fabsf(seed) * 7.31f) % 4;
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
      ccy2 = crownY + (h3 + h4 - 1.0f) * s * 0.80f * hf;
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

// Kronen-Quads auf den echten Laubpositionen (EZ-Generator). Pro Quad wird im
// Atlas ein ganzer Blattbuschel gemalt, das ist bei uns die Blattreben-Stufe.
static void emit_sprig_quads(float seed, int want){
  int variant = (int)(fabsf(seed) * 7.31f) % 4;
  static const float corners[6][2] = {
    {-0.5f,-0.5f},{0.5f,-0.5f},{-0.5f,0.5f},
    { 0.5f,-0.5f},{ 0.5f, 0.5f},{-0.5f, 0.5f}
  };
  if(ezNSprig <= 0) return;
  int stride = ezNSprig / want; if(stride < 1) stride = 1;
  for(int q = 0; q < ezNSprig && want > 0; q += stride, want--){
    float h3 = TH(97 + q * 3), h4 = TH(98 + q * 3);
    float qrot  = h3 * 6.283f;
    float qseed = h4 * 13.7f + q * 0.37f;
    for(int c = 0; c < 6; c++){
      if(nCrownVerts + 1 > CROWN_CAP) return;
      float *d = &crownVerts[nCrownVerts * CROWN_FLOATS];
      d[0] = ezSx[q]; d[1] = ezSy[q]; d[2] = ezSz[q];
      d[3] = corners[c][0]; d[4] = corners[c][1];
      d[5] = ezSsize[q]; d[6] = qrot; d[7] = qseed; d[8] = (float)variant;
      nCrownVerts++;
    }
  }
}

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

// Blatt-Orientierung aus der FLAECHENNORMALen.
// leaf.vert baut die Blattlaengsachse als (cos(t)*cos(r), -sin(t), -cos(t)*sin(r))
// und die Flaechennormale als (sin(t)*cos(r), cos(t), -sin(t)*sin(r)).
// Aufgeloest nach tilt/rotY: tilt = acos(ny), rotY = atan2(-nz, nx).
static void leaf_orient(const float n[3], float *tilt, float *rotY){
  float ny = fmaxf(-1.0f, fminf(1.0f, n[1]));
  float t = acosf(ny);
  float st = sinf(t);
  *tilt = t;
  *rotY = (st > 0.05f) ? atan2f(-n[2], n[0]) : 0.0f;
}

// ---------- Blattbuschel (Volumenkrone) ----------
// Nutzerbefund Runde 22: "grobe Aeste und direkt an den Aesten zig kleine
// Blaetter. Das erzeugt ueberhaupt keine volumetrische Baumkrone, sieht eher
// wie Tannennadeln aus."  Genau das war der Fall: leaves_along() setzte bis zu
// 700 Blaetter mit ~2 cm Radialabstand ENTLANG einer Zweigachse — eine
// Nadelbuerste.
// Vorbild (Space Colonization, Stall et al. EGWN 2007, und Crysis Leaf-Cards,
// GPU Gems 3 Kap. 16): die Krone ist eine Menge von Endpunkten im
// Kronenvolumen, und an jedem Endpunkt sitzt ein BUSCHEL, dessen Blaetter auf
// der Schale liegen und mit der Flaechennormalen nach aussen zeigen. Dann ist
// die Krone von jeder Seite geschlossen, und die Silhouette ist geproft.
static void leaf_cluster(float cx, float cy, float cz, float r, int cnt,
                         float size, float aspect, float upBias, float seed, int sb,
                         float lcr, float lcgg, float lcb){
  for(int i = 0; i < cnt; i++){
    if(nLeafInst >= LEAF_CAP) return;
    float h1 = TH(sb + i), h2 = TH(sb + i + 977), h3 = TH(sb + i + 1889);
    // gleichmaessige Richtungen ueber die Kugel (goldener Winkel + gleichmaessiges z)
    float th = (float)(sb + i) * GOLD;
    float nz0 = 1.0f - 2.0f * ((float)i + 0.5f) / (float)cnt;
    float sr = sqrtf(fmaxf(0.0f, 1.0f - nz0 * nz0));
    float dx = sr * cosf(th), dy = nz0, dz = sr * sinf(th);
    // 3/4 der Blaetter auf der Schale (sichtbare Huelle), 1/4 im Volumen
    float rr = r * (h1 < 0.75f ? (0.82f + 0.20f * h2) : (0.20f + 0.58f * h2));
    float px = cx + dx * rr, py = cy + dy * rr * 0.9f, pz = cz + dz * rr;
    // Normale: vom Buschelzentrum weg, zum Himmel gekippt (Blattchen zeigen
    // selten senkrecht nach unten)
    float nx = dx, ny = dy + upBias, nz = dz;   // upBias = planophil..erectophil
    float nl = sqrtf(nx * nx + ny * ny + nz * nz); if(nl < 1e-6f) nl = 1e-6f;
    nx /= nl; ny /= nl; nz /= nl;
    // leichte Zufallsdrehung, damit kein perfekter Igel entsteht
    float tilt, rotY;
    float nn[3] = { nx + (h2 - 0.5f) * 0.5f, ny + (h3 - 0.5f) * 0.4f, nz + (h1 - 0.5f) * 0.5f };
    leaf_orient(nn, &tilt, &rotY);
    add_leaf(px, py, pz,
             size * (0.75f + 0.5f * h3), rotY, tilt, aspect, lcr, lcgg, lcb);
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
  int   nTwig = 9 + (int)(TH(7) * 11.0f);       // 9..19 Zweige pro Ast (vorher 6..16)
  int   nFine = 2 + (int)(TH(407) * 3.0f);       // 2..4 Feinaeste pro Zweig
  float hue   = 0.20f + TH(8) * 0.20f;          // Blattfarbe individuell
  float lsat  = 0.45f + TH(9) * 0.45f;
  float lval  = 0.30f + TH(10) * 0.30f;
  float lsize = 0.085f + TH(11) * 0.085f;       // 8,5..17 cm: eine Karte steht fuer ein
                                            // Buschel kleiner Blaetter (Crysis Leaf-Cards),
                                            // sonst bleibt die Krone bei gleichem Budget loechrig
  float lasp  = 0.35f + TH(12) * 0.30f;         // Breite/Laenge (Sapling: nie 1)
  float tiltB = 0.55f + TH(13) * 0.55f;         // Blattneigung gegenueber dem Ast
  float bTone = 0.80f + TH(14) * 0.45f;         // Rindenton
  float H     = 1.5f * hf;                      // Stammhoehe in s-Einheiten
  ezDbgS = s; ezDbgHf = hf;
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
    float len = (0.60f + 0.65f * TH(40 + i)) * crownProf(shape, fr) * hf;  // groessere
                                                // Krone (Nutzer: zu klein)
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

    // ---------- Zweigspitzen = Blattbuschel ----------
    // Wird fuer ALLE Detailstufen gleich berechnet (nur die Zweig-ROHRE gibt es
    // erst ab tier 3) — die KronenFORM wechselt also nicht mit der Detailstufe.
    if(tier >= 2){
      for(int j = 0; j < nTwig; j++){
        int jj = i * 32 + j;
        float tj = 0.30f + 0.70f * (j + 0.5f) / (float)nTwig;
        float px, py, pz;
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
        // Weg vom Stamm: die Endpunkte sollen das Kronenvolumen FUELLEN statt der
        // Astachse entlangzulaufen (Space Colonization: der Ast waechst zum
        // Attraktor im freien Raum). Radiale Komponente beimischen.
        float rl = sqrtf(px * px + pz * pz); if(rl < 1e-4f) rl = 1e-4f;
        const float mix = 0.45f;
        d3x = d3x * (1.0f - mix) + (px / rl) * mix;
        d3z = d3z * (1.0f - mix) + (pz / rl) * mix;
        float dl = sqrtf(d3x * d3x + d3y * d3y + d3z * d3z); if(dl < 1e-6f) dl = 1e-6f;
        d3x /= dl; d3y /= dl; d3z /= dl;
        float len3 = len * (0.22f + 0.22f * TH(220 + jj));
        float tipx = bx + s * (px + d3x * len3);
        float tipy = by + s * (py + d3y * len3);
        float tipz = bz + s * (pz + d3z * len3);
        if(tier >= 3){
          float back = br[1].r / s * 0.9f;
          Ring tw[2];
          memset(tw, 0, sizeof tw);
          tw[0].p[0] = bx + s * (px - d3x * back); tw[0].p[1] = by + s * (py - d3y * back); tw[0].p[2] = bz + s * (pz - d3z * back);
          tw[0].r = s * 0.012f * hf;
          tw[1].p[0] = tipx; tw[1].p[1] = tipy; tw[1].p[2] = tipz;
          tw[1].r = s * 0.006f * hf;
          ring_basis(&tw[0], d3x, d3y, d3z);
          ring_transport(&tw[1], &tw[0], d3x, d3y, d3z, d3x, d3y, d3z);
          emit_tube(&tw[0], &tw[1], 4, 1.0f, 1.0f, 0.40f * bTone, 0.32f * bTone, 0.20f * bTone, 0.9f, 0.0f);
        }
        float cr2 = s * (0.105f + 0.070f * hf) * (0.80f + 0.55f * TH(230 + jj));
        int per = tier >= 3 ? 120 : 55;
        leaf_cluster(bx + s * (px + d3x * len3 * 0.60f),
                     by + s * (py + d3y * len3 * 0.60f),
                     bz + s * (pz + d3z * len3 * 0.60f),
                     cr2 * 0.80f, per, lsize, lasp, tiltB, seed,
                     3000 + jj * 1000, lcr, lcgg, lcb);
        leaf_cluster(tipx, tipy, tipz, cr2, per + per / 2,
                     lsize, lasp, tiltB, seed, 3500 + jj * 1000, lcr, lcgg, lcb);
        // ---------- Feinaeste: der Zweig gabelt sich 2..4 mal ----------
        // Genau diese Stufe fehlt: ohne sie stehen auf jedem Ast nur wenige
        // Zweige mit je einer Buschelkugel ("Cheerleader"). Die Feinaeste fuellen
        // das Kronenvolumen und tragen eigene, kleinere Buschel.
        for(int f = 0; f < nFine; f++){
          int ff = jj * 8 + f;
          float tf = 0.40f + 0.58f * (f + 0.5f) / (float)nFine;   // Ansatz am Zweig
          float ax = px + d3x * len3 * tf, ay = py + d3y * len3 * tf, az2 = pz + d3z * len3 * tf;
          float faz = az0 + ff * GOLD + (TH(600 + ff) - 0.5f) * 1.30f;
          float fae = el3 - 0.15f + TH(620 + ff) * 0.55f - (shape == 3 ? 0.35f : 0.0f);
          float fdx = cosf(fae) * cosf(faz), fdy = sinf(fae), fdz = cosf(fae) * sinf(faz);
          float rl2 = sqrtf(ax * ax + az2 * az2); if(rl2 < 1e-4f) rl2 = 1e-4f;
          fdx = fdx * 0.55f + (ax / rl2) * 0.45f;      // vom Stamm weg
          fdz = fdz * 0.55f + (az2 / rl2) * 0.45f;
          fdy = fdy * 0.75f + 0.25f;                   // leichte Aufwaertskante
          float fl = sqrtf(fdx * fdx + fdy * fdy + fdz * fdz); if(fl < 1e-6f) fl = 1e-6f;
          fdx /= fl; fdy /= fl; fdz /= fl;
          float fen = len3 * (0.38f + 0.34f * TH(640 + ff));
          float ftx = bx + s * (ax + fdx * fen), fty = by + s * (ay + fdy * fen),
                ftz = bz + s * (az2 + fdz * fen);
          if(tier >= 3){
            Ring fw[2];
            memset(fw, 0, sizeof fw);
            fw[0].p[0] = bx + s * ax; fw[0].p[1] = by + s * ay; fw[0].p[2] = bz + s * az2;
            fw[0].r = s * 0.006f * hf;
            fw[1].p[0] = ftx; fw[1].p[1] = fty; fw[1].p[2] = ftz;
            fw[1].r = s * 0.003f * hf;
            ring_basis(&fw[0], fdx, fdy, fdz);
            ring_transport(&fw[1], &fw[0], fdx, fdy, fdz, fdx, fdy, fdz);
            emit_tube(&fw[0], &fw[1], 3, 1.0f, 1.0f, 0.42f * bTone, 0.34f * bTone, 0.21f * bTone, 0.9f, 0.0f);
          }
          leaf_cluster(ftx, fty, ftz,
                       cr2 * (0.55f + 0.25f * TH(660 + ff)),
                       tier >= 3 ? 60 : 30,
                       lsize, lasp, tiltB, seed, 7000 + ff * 977, lcr, lcgg, lcb);
        }
      }
      // Buschel auf der Astspitze — die groessten, sie bilden den Kronensaum
      leaf_cluster(bx + s * t0x, by + s * t0y, bz + s * t0z,
                   s * (0.13f + 0.09f * hf) * (0.85f + 0.4f * TH(57 + i)),
                   tier >= 3 ? 430 : 170,
                   lsize, lasp, tiltB, seed, 900 + i * 7000, lcr, lcgg, lcb);
    }
  }

  // ---------- Kronen-Quads: sitzen um die echten Astspitzen ----------
  emit_crown_quads(bx, bz, s, hf, seed, by + s * H * 1.15f, tipx, tipy, tipz, nTip);
}
// ---------- Generator B: Port des EZ-Tree-Algorithmus --------------------
// Uebernommen aus github.com/dgreenheck/ez-tree (MIT License, (c) 2024 Daniel
// Greenheck) — der Algorithmus, nicht der Code. Kernpunkte:
//   * ein Ast = Kette von Abschnitten (sections), jeder mit Position, Rahmen
//     und Radius; der Ast waechst entlang seiner lokalen Y-Achse
//   * pro Abschnitt Gnarliness-Jitter, skaliert mit 1/sqrt(Radius) -> duenne
//     Aeste bewegen sich staerker als dicke
//   * Wachstums-Kraft: Drehung um cross(Astachse, Ziel) mit Schritt
//     strength/Radius, begrenzt auf den vollen Winkel -> duenne Aeste biegen
//     staerker nach oben (Phototropismus). Das ist der Grund, warum deren Aeste
//     geschwungen sind und unsere gerade Stoecke waren.
//   * Kinder geschichtet entlang der Elternachse (Slot + Jitter) und auf
//     Radial-Slots verteilt (Slotzuweisung vertauscht, +/- halber Slot Jitter).
//     Deren Begruendung im Code: sonst drehen Nadelbaeume ihre laengsten Aeste
//     auf eine Seite.
//   * Kinder-Radius IMMER aus dem Elternradius interpoliert * Level-Faktor
//   * Laub: gleiche Schichtung an den Terminalaesten
// Die Parameterwerte stammen direkt aus deren Presets (*_medium.json).

typedef struct { Ring rg; float d[3]; } Sec;

typedef struct {
  const char *name;
  int   levels, evergreen;
  int   children[4], sections[4], segments[4];
  float angle[4], length[4], radius[4], start[4], gnarl[4], twist[4], taper[4];
  float force, leafCount, leafAngle, leafStart, leafSize;
} EzPreset;

static const EzPreset EZ_PRESETS[] = {
  // name  lvl ev  children      sections      segments
  { "oak",   3, 0, {6,4,3,0},  {8,6,3,1},    {7,5,3,3},
    {0,54,58,32},   {37.24f,11.08f,12.39f,7.16f}, {1.41f,0.90f,0.69f,1.19f},
    {0,0.49f,0.06f,0.12f}, {0,-0.10f,-0.15f,0.09f}, {-0.23f,0.42f,0,0},
    {0.73f,0.42f,0.69f,0.75f}, 0.020f, 18, 42, 0.16f, 2.50f },
  { "ash",   3, 0, {7,4,3,0},  {12,8,6,4},   {12,6,4,3},
    {0,48,75,60},   {43.47f,27.14f,9.51f,4.60f}, {2.00f,0.63f,0.76f,0.70f},
    {0,0.23f,0.33f,0},     {0.03f,0.25f,0.20f,0.09f}, {0.09f,-0.07f,0,0},
    {0.70f,0.70f,0.70f,0.70f}, 0.010f, 16, 55, 0.00f, 2.67f },
  { "aspen", 2, 0, {10,3,3,0}, {12,10,8,6},  {8,6,4,3},
    {0,75,32,7},    {50.0f,6.07f,11.19f,1.0f},  {0.72f,0.41f,0.70f,0.70f},
    {0,0.59f,0.35f,0},     {0.05f,0.12f,0.12f,0.02f}, {0,0,0,0},
    {0.37f,0.13f,0.70f,0.70f}, 0.0148f, 11, 30, 0.124f, 2.50f },
  { "pine",  1, 1, {82,3,5,0}, {12,10,8,6},  {8,6,4,3},
    {0,110,16,60},  {50.0f,23.87f,14.08f,1.0f}, {1.05f,0.36f,0.70f,0.70f},
    {0,0.27f,0.14f,0.30f}, {0.05f,0.08f,0,0},     {0,0,0,0},
    {0.70f,0.70f,0.70f,0.70f}, -0.003f, 30, 39, 0.09f, 1.435f },
};
#define EZ_NPRESETS ((int)(sizeof(EZ_PRESETS) / sizeof(EZ_PRESETS[0])))

// Drehung eines Vektors um eine Achse (Rodrigues)
static void ez_rot(float *x, float *y, float *z,
                   float ax, float ay, float az, float ang){
  float c = cosf(ang), s = sinf(ang), k = 1.0f - c;
  float vx = *x, vy = *y, vz = *z;
  float kd = ax * vx + ay * vy + az * vz;
  float cx = ay * vz - az * vy, cy = az * vx - ax * vz, cz = ax * vy - ay * vx;
  *x = vx * c + cx * s + ax * kd * k;
  *y = vy * c + cy * s + ay * kd * k;
  *z = vz * c + cz * s + az * kd * k;
}

static void ez_norm3(float *x, float *y, float *z){
  float l = sqrtf((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
  if(l < 1e-6f){ *x = 0.0f; *y = 1.0f; *z = 0.0f; return; }
  *x /= l; *y /= l; *z /= l;
}

// rekursiver Ast: Abschnitte bauen, Roehre senden, Kinder/Laub ansetzen
static void ez_grow(int lvl, const Sec *base, const EzPreset *P, float unit,
                    float seed, int tier, float btone){
  int S = P->sections[lvl], G = P->segments[lvl];
  // Detailstufen: fern weniger Abschnitte/Segmente und flachere Hierarchie
  if(tier == 2){ S = (S + 1) / 2; G = (G * 3) / 4; }
  else if(tier == 0){ S = (lvl == 0) ? 3 : 1; G = 4; }
  if(S < 1) S = 1;
  if(S > 12) S = 12;
  if(G < 3) G = 3;
  if(G > 8) G = 8;

  // Kronenlevel-Laengen sind extra skalierbar: ihre Presets sind schlanker als
  // unsere Kronen proportioniert (Breite/Hoehe ~0,4 statt ~0,85).
  float len = P->length[lvl] * unit * (lvl == 0 ? 1.0f : treeEzSpread);
  float sl = len / (float)S;
  Sec sec[13];
  sec[0] = *base;

  for(int i = 1; i <= S; i++){
    sec[i] = sec[i-1];
    float dx = sec[i-1].d[0], dy = sec[i-1].d[1], dz = sec[i-1].d[2];
    // Gnarliness: duenne Aeste staerker (deren Faktor 1/sqrt(Radius)). Der Radius
    // muss DAFUER in Preset-Einheiten stehen: unser Baum ist gegenueber deren
    // Presets um 'unit' geschrumpft, sonst wirkt die Formel um 1/unit zu stark.
    float rpu = sec[i-1].rg.r / unit;          // Radius in Preset-Einheiten
    float g = P->gnarl[lvl] * fmaxf(1.0f, 1.0f / sqrtf(fmaxf(rpu, 0.001f)));
    if(g != 0.0f){
      ez_rot(&dx, &dy, &dz, sec[i-1].rg.u[0], sec[i-1].rg.u[1], sec[i-1].rg.u[2],
             (TH(40 + i) - 0.5f) * 2.0f * g);
      ez_rot(&dx, &dy, &dz, sec[i-1].rg.v[0], sec[i-1].rg.v[1], sec[i-1].rg.v[2],
             (TH(90 + i) - 0.5f) * 2.0f * g);
    }
    // Wachstums-Kraft nach oben (negatives force biegt nach unten): Schritt =
    // force / Radius, begrentzt auf den vollen Winkel zur Zielrichtung
    if(P->force != 0.0f){
      float ax = -dz, az = dx;                 // axis = cross(d, up), up = (0,1,0)
      float sf = sqrtf(ax * ax + az * az);
      if(sf > 1e-6f){
        float full = atan2f(sf, dy);           // dy = dot(d, up)
        float step = P->force / fmaxf(rpu, 0.02f);
        if(step >  full) step =  full;
        if(step < -full) step = -full;
        ez_rot(&dx, &dy, &dz, ax / sf, 0.0f, az / sf, step);
      }
    }
    ez_norm3(&dx, &dy, &dz);
    sec[i].d[0] = dx; sec[i].d[1] = dy; sec[i].d[2] = dz;
    sec[i].rg.p[0] = sec[i-1].rg.p[0] + dx * sl;
    sec[i].rg.p[1] = sec[i-1].rg.p[1] + dy * sl;
    sec[i].rg.p[2] = sec[i-1].rg.p[2] + dz * sl;
    // Radius exakt nach deren Regel: Basisradius * (1 - taper[level] * i/S);
    // Nadelbaum (evergreen) bis auf 0, weil er keinen Terminalast hat. Der letzte
    // Abschnitt des letzten Levels geht auf ~0 (deren closeTip-Verhalten).
    float fr = (float)i / (float)S;
    float r = base->rg.r * (P->evergreen ? (1.0f - fr) : (1.0f - P->taper[lvl] * fr));
    if(lvl >= P->levels && i == S) r = 0.001f;
    sec[i].rg.r = r < 0.001f ? 0.001f : r;
    // Rahmen parallel transportieren (gleiches Verfahren wie beim Stamm)
    ring_transport(&sec[i].rg, &sec[i-1].rg,
                   sec[i-1].d[0], sec[i-1].d[1], sec[i-1].d[2], dx, dy, dz);
    if(P->twist[lvl] != 0.0f){
      ez_rot(&sec[i].rg.u[0], &sec[i].rg.u[1], &sec[i].rg.u[2], dx, dy, dz, P->twist[lvl]);
      ez_rot(&sec[i].rg.v[0], &sec[i].rg.v[1], &sec[i].rg.v[2], dx, dy, dz, P->twist[lvl]);
    }
    // Rindenfarbe pro Level (Stamm dunkler, aeussere Zweige gruenlicher)
  }

  // ---- Kinder: geschichtet entlang der Achse, Radial-Slots vertauscht ----
  int maxKid = (tier >= 3) ? P->levels : (tier == 2 ? P->levels - 1 : 0);
  Sec kids[96]; int nkid = 0;
  if(lvl < maxKid){
    int cnt = P->children[lvl];
    if(tier == 2) cnt = (cnt * 3) / 4;
    if(cnt < 1) cnt = 1;
    if(cnt > 96) cnt = 96;
    float startMin = P->start[lvl + 1];
    float stp = (1.0f - startMin) / (float)cnt;
    int slots[96];
    for(int i = 0; i < cnt; i++) slots[i] = i;
    for(int i = cnt - 1; i > 0; i--){
      int j = (int)(TH(300 + i) * (float)(i + 1));
      if(j > i) j = i;
      int t = slots[i]; slots[i] = slots[j]; slots[j] = t;
    }
    float radialOff = TH(310);
    float ang = P->angle[lvl + 1] * 0.017453293f;
    for(int i = 0; i < cnt; i++){
      float t = startMin + (i + TH(320 + i)) * stp;
      if(t > 1.0f) t = 1.0f;
      float fs = t * (float)S; int si = (int)fs; if(si > S - 1) si = S - 1;
      float al = fs - (float)si;
      Sec cb;
      for(int k = 0; k < 3; k++)
        cb.rg.p[k] = sec[si].rg.p[k] + (sec[si+1].rg.p[k] - sec[si].rg.p[k]) * al;
      // Kinder-Radius IMMER aus dem Elternast abgeleitet
      float rp = sec[si].rg.r + (sec[si+1].rg.r - sec[si].rg.r) * al;
      cb.rg.r = rp * P->radius[lvl + 1];
      // Elternrahmen bei t (u/v interpolieren, dann orthonormieren)
      float ux = sec[si].rg.u[0] + (sec[si+1].rg.u[0] - sec[si].rg.u[0]) * al;
      float uy = sec[si].rg.u[1] + (sec[si+1].rg.u[1] - sec[si].rg.u[1]) * al;
      float uz = sec[si].rg.u[2] + (sec[si+1].rg.u[2] - sec[si].rg.u[2]) * al;
      float px = sec[si].d[0] + (sec[si+1].d[0] - sec[si].d[0]) * al;
      float py = sec[si].d[1] + (sec[si+1].d[1] - sec[si].d[1]) * al;
      float pz = sec[si].d[2] + (sec[si+1].d[2] - sec[si].d[2]) * al;
      ez_norm3(&px, &py, &pz);
      float dd = ux * px + uy * py + uz * pz;
      ux -= px * dd; uy -= py * dd; uz -= pz * dd;
      ez_norm3(&ux, &uy, &uz);
      // 1) Neigung gegenueber dem Elternast, 2) Radial-Slot um die Elternachse
      float dx = px, dy = py, dz = pz;
      ez_rot(&dx, &dy, &dz, ux, uy, uz, ang);
      float ra = 6.2831853f * (radialOff + ((float)slots[i] + (TH(330 + i) - 0.5f)) / (float)cnt);
      ez_rot(&dx, &dy, &dz, px, py, pz, ra);
      ez_norm3(&dx, &dy, &dz);
      cb.d[0] = dx; cb.d[1] = dy; cb.d[2] = dz;
      ring_basis(&cb.rg, dx, dy, dz);
      kids[nkid] = cb; nkid++;
    }
  }

  if(lvl == 0){                       // Messbank: Stammradien pro Abschnitt
    ezDbgNSec = S;
    for(int i = 0; i <= S && i < 13; i++) ezDbgR[i] = sec[i].rg.r;
  }
  // ---- Roehren senden ----
  float tone = btone * (0.86f + 0.05f * (float)lvl);
  float cr = (0.30f - 0.03f * (float)lvl) * tone;
  float cg = (0.24f + 0.03f * (float)lvl) * tone;
  float cb2 = (0.15f - 0.02f * (float)lvl) * tone;
  float sw = 0.20f + 0.30f * (float)lvl;
  for(int i = 1; i <= S; i++)
    emit_tube(&sec[i-1].rg, &sec[i].rg, G, sw, sw, cr, cg, cb2, 0.9f, (float)i * sl);

  if(lvl >= maxKid){
    // Terminalast: Laub geschichtet entlang des Astes (deren Laub-Schema).
    // Laubpositionen werden in ALLEN Stufen gesammelt — die Atlas-Karten sind
    // bei fern die einzige Laubdarstellung. Nur die Einzelblaetter entfallen.
    int lc = (int)P->leafCount;
    if(tier == 0) lc = lc / 2;
    if(ezNTip < EZ_TIPCAP){              // Spitze fuer die Distanz-Quads merken
      ezTipx[ezNTip] = sec[S].rg.p[0];
      ezTipy[ezNTip] = sec[S].rg.p[1];
      ezTipz[ezNTip] = sec[S].rg.p[2];
      ezNTip++;
    }
    if(lc < 1) return;
    float lsz = P->leafSize * unit;
    float lcr = 0.10f + 0.16f * TH(700), lcgg = 0.26f + 0.26f * TH(701), lcb = 0.05f + 0.10f * TH(702);
    for(int i = 0; i < lc; i++){
      float st = (1.0f - P->leafStart) / (float)lc;
      float t = P->leafStart + (i + TH(400 + i)) * st;
      if(t > 1.0f) t = 1.0f;
      float fs = t * (float)S; int si = (int)fs; if(si > S - 1) si = S - 1;
      float al = fs - (float)si;
      float cx = sec[si].rg.p[0] + (sec[si+1].rg.p[0] - sec[si].rg.p[0]) * al;
      float cy = sec[si].rg.p[1] + (sec[si+1].rg.p[1] - sec[si].rg.p[1]) * al;
      float cz = sec[si].rg.p[2] + (sec[si+1].rg.p[2] - sec[si].rg.p[2]) * al;
      if(ezNSprig < EZ_SPRIGCAP){       // Karte fuer den Atlas-Quad
        ezSx[ezNSprig] = cx; ezSy[ezNSprig] = cy; ezSz[ezNSprig] = cz;
        ezSsize[ezNSprig] = lsz * (1.6f + 0.9f * TH(430 + i));
        ezNSprig++;
      }
      if(tier >= 3){                    // Nahstufe: Einzelblaetter zusaetzlich
        float rr = lsz * (0.30f + 0.25f * TH(430 + i));
        leaf_cluster(cx, cy, cz, rr, 5, lsz * 0.45f, 0.85f, 0.55f,
                     seed + (float)i * 7.3f, 500 + i * 3, lcr, lcgg, lcb);
      }
    }
    return;
  }

  for(int i = 0; i < nkid; i++)
    ez_grow(lvl + 1, &kids[i], P, unit,
            seed + (float)lvl * 17.0f + (float)i * 3.13f, tier, btone);
}

static void gen_tree_ez(float bx, float by, float bz, float s, float seed, int tier){
  int pidx = (int)(TH(0) * (float)(EZ_NPRESETS - 0.001f));
  if(treeEzPreset >= 0 && treeEzPreset < EZ_NPRESETS) pidx = treeEzPreset;
  const EzPreset *P = &EZ_PRESETS[pidx];
  float hf = 0.80f + TH(1) * 2.20f;
  // dieselbe Groessenlogik wie Generator A: Kronenmittelpunkt-Hoehe ueber Basis
  float treeH = 1.5f * hf * 1.15f * s;
  float unit = treeH / P->length[0];
  float btone = 0.75f + 0.55f * TH(2);
  Sec root;
  root.rg.p[0] = bx; root.rg.p[1] = by; root.rg.p[2] = bz;
  root.rg.r = P->radius[0] * unit;
  root.d[0] = 0.0f; root.d[1] = 1.0f; root.d[2] = 0.0f;
  ring_basis(&root.rg, 0.0f, 1.0f, 0.0f);
  ezNTip = 0; ezNSprig = 0;
  ezDbgName = P->name; ezDbgUnit = unit; ezDbgR0 = root.rg.r; ezDbgNSec = 0;  ezDbgS = s; ezDbgHf = hf;
  ez_grow(0, &root, P, unit, seed, tier, btone);
  // Atlas-Karten: fern mehr (sie sind das Laub), nah weniger (dort stehen
  // Einzelblaetter). Budget: CROWN_CAP erlaubt ~15000 Quads pro Region.
  emit_sprig_quads(seed, tier >= 3 ? 24 : (tier == 2 ? 60 : 22));
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
  // DEMO_ONETREE: nur der naechste Baum (Liste ist gerade nach Entfernung
  // sortiert), damit ein einzelner Baum in Ruhe betrachtet werden kann.
  if(treeOneShot && nc > 1) nc = 1;
  for(int i = 0; i < nc; i++){
    // Detail-Stufe nach Entfernung zum Kameraziel: sehr nahe Baeume bekommen
    // Zweige + vollstaendige Blaetter (bis ~100.000), mittlere nur Blaetter an
    // den Aesten, ferne nur die Atlas-Quads.
    int tier = treeForceTier >= 0 ? treeForceTier
           : (cand[i].d2 < 121.0f ? 3 : (cand[i].d2 < 784.0f ? 2 : 0));
    float tseed = cand[i].p[0] * 12.9898f + cand[i].p[2] * 78.233f;
    if(treeGenMode) gen_tree_ez(cand[i].p[0], cand[i].p[1], cand[i].p[2], cand[i].s, tseed, tier);
    else            gen_tree (cand[i].p[0], cand[i].p[1], cand[i].p[2], cand[i].s, tseed, tier);
    nTrees++;
  }
}
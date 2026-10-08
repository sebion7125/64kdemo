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
#include <pthread.h>   // Baum-Build im Worker-Thread (Muster wie grass_cpu.c)
#include <math.h>
#include <stdio.h>   // Baum-Stufen-Log (DEMO_TREELOG)
#include <string.h>

int   treeOneShot = 0;
int   treeGenMode = 1;   // 0 = eigener Generator, 1 = EZ-Tree-Port
int   treeEzPreset = -1;   // DEMO_EZPRESET=0..3: Preset erzwingen (-1 = pro Baum gewuerfelt)
float treeEzSpread = 1.0f;  // DEMO_EZSPREAD: Laengenfaktor der Kronenlevel (1 = 1:1-Port)
const char *ezDbgName = "-"; float ezDbgUnit = 0.0f, ezDbgR0 = 0.0f;
float ezDbgR[13]; int ezDbgNSec = 0;
float ezDbgS = 0.0f, ezDbgHf = 0.0f;
int   treeForceTier = -1;
int   treeTierLog = 0;
// Cap-Waechter: eine Stufe, die mitten im Baum abbricht, sieht aus wie ein
// abgesaegter Stamm ohne Laub. Deshalb wird jeder Cap-Treffer gezaehlt.
int   treeCapTree = 0, treeCapCrown = 0, treeCapLeaf = 0;
int   treeSkipped = 0;   // Baeume, die nicht ins Budget passten
int   treeCapSprig = 0;   // Laub-Liste pro Baum voll
float treeLodScale = 0.0f;   // DEMO_LODRSCALE (0 = aus)
float treeLodRef[3] = { 0.0f, 0.0f, 0.0f };
int   treeLodRefValid = 0;
float treeFocus[3] = { 0.0f, 0.0f, 0.0f };
int   treeFocusValid = 0;
// Detail-Radius des gerade gebauten Baums (lodR in crownVerts/leafInst). Steht
// hier, weil add_leaf()/leaf_cluster() von beiden Generatoren aus tief unten
// aufgerufen werden und die Kette dafuer nicht voll durchgereicht wird.
static float gLodR = 30.0f;
static float gLodR2 = 0.0f;
static float gBbSize = 0.0f, gBbCen[3] = {0,0,0};   // BBox des letzten Baums
static float ezLszMax = 0.0f;  // groesster Blattgrundradius dieses Baums
static int   gBbWant = 0;      // Billboard fuer diesen Baum wirklich zeichnen
                        // sitzt pro Karte in der Instanz, damit der Uebergang
                        // Karten -> Ganzbaum-Billboard im Shader kontinuierlich
                        // ueber die Entfernung laufen kann (kein Stufensprung).
// Doppel-Puffer (Runde 26): der Build laeuft im Worker-Thread und schreibt in
// die Schreibseite, gezeichnet wird ungestoert aus der Zeichenseite. Getauscht
// wird in trees_build_finish() unter Mutex — nie mitten im Build.
static float tvA[TREE_VERT_CAP * TREE_FLOATS], tvB[TREE_VERT_CAP * TREE_FLOATS];
static float cvA[CROWN_CAP * CROWN_FLOATS],    cvB[CROWN_CAP * CROWN_FLOATS];
static float lvA[LEAF_CAP * LEAF_FLOATS],      lvB[LEAF_CAP * LEAF_FLOATS];
float *treeVerts = tvA, *crownVerts = cvA, *leafInst = lvA;      // Schreibseite (Build)
float *treeVertsD = tvB, *crownVertsD = cvB, *leafInstD = lvB;   // Zeichenseite (Main)
float wireVerts[WIRE_CAP * WIRE_FLOATS];
int treeBuildRange = 20;   // DEMO_TREEBUILDRANGE: Zellen-Radius des Wald-Baus
int nTreeVerts = 0, nCrownVerts = 0, nWireVerts = 0, nTrees = 0, nLeafInst = 0;   // Schreibseite
int nTreeVertsD = 0, nCrownVertsD = 0, nLeafInstD = 0, nTreesD = 0;               // Zeichenseite

// Thread-Zustand (Mutex wie beim Gras: der Main-Thread tauscht nur Zeiger/Counts,
// der Worker beruehrt keine GL-Objekte)
static pthread_mutex_t treeMu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  treeCv = PTHREAD_COND_INITIALIZER;
static int treeReqPending = 0, treeBuildDone = 0, treeBuilding = 0;
static float treeReqTX = 0.0f, treeReqTZ = 0.0f, treeReqLod[3];
static int treeReqLodValid = 0;

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
      if(nTreeVerts + 1 > TREE_VERT_CAP){ treeCapTree++; return; }
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
      if(nTreeVerts + 1 > TREE_VERT_CAP){ treeCapTree++; return; }
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
// 512 war zu klein: ein oak-Terminalast-Schema (6x4x3 = 72 Aeste x 18 Blaetter)
// hat 1296 Laubpositionen, pine 82 x 30 = 2460. Mit 512 wurden nur die ersten
// ~28 Aeste erfasst -> die Karten einer fernen Krone lagen auf einem Fleck.
#define EZ_SPRIGCAP 16384   // groesster Baum: 4 920 Laubansaetze (1 230 Karten bei Stride 4)
static float ezSx[EZ_SPRIGCAP], ezSy[EZ_SPRIGCAP], ezSz[EZ_SPRIGCAP], ezSsize[EZ_SPRIGCAP];
static int   ezNSprig = 0;
static int   ezBucket = 0;   // Impostor-Eimer dieses Baums (0..IMP_TILES-1)
// Kinderzahl-Faktor (1:1 = 100). DEMO_KIDPCT erlaubt Messungen. Wirkt auf alle
// Stufen gleich, die Aststruktur wechselt also nicht mit der Detailstufe.
int ezKidPct = 100;
// Einzelblaetter fuer den aktuellen Baum erzeugen? Haengt an der Entfernung
// (Band um lodR), nicht an der Build-Stufe — sonst fehlt in einem Band um lodR
// die Geometrie fuer die Ueberblendung und der Baum steht kahl da.
static int ezWantLeaves = 1;
// Kartengroesse der aktuellen Stufe (deren leafScale) und Karten-Stride.
static float ezCardScale = 1.25f;
// DEMO_CARDT0="<div> <scale>": Karten pro Baum (ezNSprig/div) und Kartengroesse
// der groebsten Stufe — Sweep-Bank, damit die ferne Kronendeckung messbar an die
// nahe angepasst werden kann.
int treeCardDiv0 = 3; float treeCardScale0 = 1.45f;   // gemessen: deckt wie tier 3
// Basisstufe der Krone (tier 2/3). Gemessen an einem Baum, Deckung = gruenen
// Pixel / bbox-Flaeche, Gras+Schilf aus: div 2 / scale 1,25 -> 38 % auf 25 m und
// 39 % auf 40 m. div 3 / scale 2,0 -> 50 % / 52 % bei 18 % WENIGER Karten
// (287 194 -> 234 129 im Wald). Wenige grosse Karten schlagen viele kleine.
// Einzelblatt-Groesse/-Anzahl pro Buschel. Gemessen an einem 3,9-m-Baum,
// Laubpixel bei 10 m: scale 1,0 -> 6 639, scale 2,0 -> 14 066, scale 3,0 ->
// 18 652. Bei 1,0 sind die Blaetter 3-5 cm gross und die Krone durchsichtig;
// 2,0 (6-10 cm) entspricht echten Eichen-/Eschenblaettern und fuellt sie.
struct ImpRep treeImp[IMP_TILES];
static struct ImpRep impRepD[IMP_TILES];
int impNTree[IMP_TILES];   // Schreibseite des laufenden Builds
float ezLeafScale = 2.0f, ezLeafPer = 1.0f;   // DEMO_LEAFSC / DEMO_LEAFPER
int treeCardDivN = 3; float treeCardScaleN = 2.00f;    // DEMO_CARDT2 "<div> <scale>" 
// DEMO_T2="<stride> <Gzaehler> <Gnenner>": Vernetzung der mittleren Detailstufe.
// Sweep-Bank, um Geruest-Kosten gegen Silhouette-Qualitaet zu messen.
int treeT2Stride = 3, treeT2Gnum = 3, treeT2Gden = 4;
int treeDowngraded = 0;   // Baeume, die wegen Budget auf die groebste Stufe fielen
int treeBB0Cards = 0;    // Zusatzkarten unter dem tier-0-Billboard (DEMO_BB0CARDS)
int treeBillboard0 = 1;      // DEMO_NOBB0=1: tier 0 wieder mit Einzelkarten
// Blattfarbe des gerade gebauten Baumes. Der EZ-Generator wuerfelte sie vorher
// pro Terminalast (0,10..0,26 / 0,26..0,52 / 0,05..0,15) — deshalb sahen die
// Distanz-Karten nach einem ANDEREN Baum aus als das Nah-LOD. Jetzt: eine Farbe
// pro Baum (HSV wie im alternativen Generator), pro Ast nur noch +/-15 %
// moduliert, und dieselbe Farbe steht in der Karten-Instanz.
static float ezLeafR = 0.18f, ezLeafG = 0.46f, ezLeafB = 0.09f;

// Blatt-PALETTE (HSV-Bereiche pro Baum). Der Nutzer mag die alte Palette nicht
// (hue 72..144 Grad enthaelt die olivigen/gelben Randwerte -> "herbstlich").
// Kompilierter Standard = "wald": hue 101..130 Grad, gemessen gerendert
// (70,128,47), Saettigung 0,63, G/R 1,82 — gegen (72,110,29)/G-R 1,53 vorher.
// Justierbar ohne Neukompilieren:
//   DEMO_LEAFPAL=aktuell|saftig|frisch|tief|wald
//   DEMO_LEAFHSV="hmin hmax smin smax vmin vmax"   (0..1)
float ezHueMin = 0.28f, ezHueMax = 0.36f;
float ezSatMin = 0.65f, ezSatMax = 0.88f;
float ezValMin = 0.38f, ezValMax = 0.58f;

// Kronen-Quads (Distanz-LOD): 2/3 an den echten Astspitzen, Rest um die
// Kronenmitte. Steht beiden Generatoren zur Verfuegung.
static void emit_crown_quads(float bx, float bz, float s, float hf,
                             float seed, float crownY,
                             const float *tipx, const float *tipy, const float *tipz, int nTip){
  int K = 12;
  int variant = (int)(fabsf(seed) * 7.31f) % 4;
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
    // Eine Instanz pro Karte (Runde 28). Vorher 6 Eckpunkt-Instanzen a 13 Float
    // = 312 Byte pro Karte; die Ecke kommt jetzt aus einem statischen 6-Vertex-
    // Quad im Vertex-Array. Ersparnis: 170 MB -> 24 MB Puffer, ~6x weniger
    // Upload und Build-Daten.
    if(nCrownVerts + 1 > CROWN_CAP){ treeCapCrown++; return; }
    { float *d = &crownVerts[nCrownVerts * CROWN_FLOATS];
      d[0] = cc; d[1] = ccy2; d[2] = ccz2;
      d[3] = qsize; d[4] = qrot; d[5] = qseed; d[6] = (float)variant;
      d[7] = gLodR; d[8] = ezLeafR; d[9] = ezLeafG; d[10] = ezLeafB; d[11] = gLodR2;
      nCrownVerts++; }
  }
}

// Ganzbaum-Billboard fuer die groebste Detailstufe (TODO 23): ein Quad pro Baum
// statt hunderter Karten. Groesse/Mitte werden aus den echten Laubpositionen
// abgeleitet, damit die Kachel die Krone dieses Baumes abdeckt. Die Kachel ist
// Variante 4..7 (crown.frag nimmt dafuer eine andere Textur mit 2x2 Kacheln).
static void emit_crown_billboard(float seed, float bx, float by, float bz, float treeH){
  gBbSize = 0.0f;              // pro Baum neu — sonst gilt die BBox des letzten
  if(ezNSprig <= 0) return;
  float mn[3] = { 1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f,-1e30f,-1e30f};
  // Reichweite pro Laubposition: Karte ODER Blattbuschel, je nachdem was weiter
  // reicht. Nur die Kartenausdehnung zu nehmen schneidet oben Blaetter ab —
  // der Blattbuschel reicht bis ~2x Blattgrundgroesse ueber die Position
  // (Nutzerbefund: "viele Quads nach oben abgeschnitten").
  float lr = ezLszMax * 2.0f;
  for(int i = 0; i < ezNSprig; i++){
    float r = ezSsize[i] * 0.5f;
    if(lr > r) r = lr;
    if(ezSx[i] - r < mn[0]) mn[0] = ezSx[i] - r;
    if(ezSx[i] + r > mx[0]) mx[0] = ezSx[i] + r;
    if(ezSy[i] - r < mn[1]) mn[1] = ezSy[i] - r;
    if(ezSy[i] + r > mx[1]) mx[1] = ezSy[i] + r;
    if(ezSz[i] - r < mn[2]) mn[2] = ezSz[i] - r;
    if(ezSz[i] + r > mx[2]) mx[2] = ezSz[i] + r;
  }
  // Der Quad muss STAMM + Krone abdecken — sonst schwebt in der Ferne ein
  // gruener Fleck ohne Baum (Nutzerbefund).
  if(bx < mn[0]) mn[0] = bx; if(bx > mx[0]) mx[0] = bx;
  if(bz < mn[2]) mn[2] = bz; if(bz > mx[2]) mx[2] = bz;
  if(by < mn[1]) mn[1] = by;
  float top = by + treeH; if(top > mx[1]) mx[1] = top;
  float size = mx[0] - mn[0];
  if(mx[2] - mn[2] > size) size = mx[2] - mn[2];
  if(mx[1] - mn[1] > size) size = mx[1] - mn[1];
  if(size < 0.5f) return;
  // 1.15: der Quad ist etwas grosser als der Baum. Ist er exakt so gross,
  // beruehrt die Krone den Quad-Rand und man sieht gegen den Himmel eine gerade
  // Kante (gemessen: 14 von 64 Kacheln randberuehrend).
  size *= 1.15f;
  // BBox UNABHAENGIG davon setzen, ob dieser Baum ein Billboard bekommt: die
  // Aufnahme-Rahmung braucht sie fuer jeden Repraesentanten. Tier-3-Baeume
  // zeichnen selbst kein Billboard — ohne das fiel die Rahmung auf 0,75*H
  // zurueck und schnitt die Krone ab (Nutzer: "viele Quads nach oben abgeschnitten").
  gBbSize = size;
  gBbCen[0] = (mn[0] + mx[0]) * 0.5f; gBbCen[1] = (mn[1] + mx[1]) * 0.5f;
  gBbCen[2] = (mn[2] + mx[2]) * 0.5f;
  if(!gBbWant) return;         // Rahmung ist gesetzt, Quad aber nicht gewollt
  if(nCrownVerts + 1 > CROWN_CAP){ treeCapCrown++; return; }
  float *d = &crownVerts[nCrownVerts * CROWN_FLOATS];
  d[0] = (mn[0] + mx[0]) * 0.5f; d[1] = (mn[1] + mx[1]) * 0.5f; d[2] = (mn[2] + mx[2]) * 0.5f;
  d[3] = size; d[4] = 0.0f; d[5] = seed;
  d[6] = 4.0f + (float)ezBucket;   // 4..67 = Impostor-Kachel (8x8-Atlas)
  d[7] = gLodR; d[8] = ezLeafR; d[9] = ezLeafG; d[10] = ezLeafB; d[11] = gLodR2;
  nCrownVerts++;
}

// Kronen-Quads auf den echten Laubpositionen (EZ-Generator). Pro Quad wird im
// Atlas ein ganzer Blattbuschel gemalt, das ist bei uns die Blattreben-Stufe.
static void emit_sprig_quads(float seed, int want){
  int variant = (int)(fabsf(seed) * 7.31f) % 4;
  if(ezNSprig <= 0 || want <= 0) return;
  int stride = ezNSprig / want; if(stride < 1) stride = 1;
  for(int q = 0; q < ezNSprig && want > 0; q += stride, want--){
    float h3 = TH(97 + q * 3), h4 = TH(98 + q * 3);
    float qrot  = h3 * 6.283f;
    float qseed = h4 * 13.7f + q * 0.37f;
    if(nCrownVerts + 1 > CROWN_CAP){ treeCapCrown++; return; }
    { float *d = &crownVerts[nCrownVerts * CROWN_FLOATS];
      d[0] = ezSx[q]; d[1] = ezSy[q]; d[2] = ezSz[q];
      d[3] = ezSsize[q]; d[4] = qrot; d[5] = qseed; d[6] = (float)variant;
      d[7] = gLodR; d[8] = ezLeafR; d[9] = ezLeafG; d[10] = ezLeafB; d[11] = gLodR2;
      nCrownVerts++; }
  }
}

// Einzelblatt-Instanz: Basispunkt (Welt), Groesse, Orientierung, Seitenverhaeltnis, Farbe
static void add_leaf(float x, float y, float z, float size, float rotY, float tilt,
                     float aspect, float cr, float cg, float cb){
  if(nLeafInst >= LEAF_CAP){ treeCapLeaf++; return; }
  float *d = &leafInst[nLeafInst * LEAF_FLOATS];
  d[0] = x; d[1] = y; d[2] = z;
  d[3] = size; d[4] = rotY; d[5] = tilt; d[6] = aspect;   // aspect = Breite/Laenge
  d[7] = cr; d[8] = cg; d[9] = cb; d[10] = gLodR;
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
    if(nLeafInst >= LEAF_CAP){ treeCapLeaf++; return; }
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
  ezLeafR = lcr; ezLeafG = lcgg; ezLeafB = lcb;   // auch fuer die Distanz-Karten

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
#define EZ_NPRESETS_BUCKETS (IMP_TILES / EZ_NPRESETS)   // Formen pro Art

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
// secOv/segOv: deren Terminalast-Regel ueberschreibt Section- und Segmentzahl des
// Fortsetzungszweigs mit der des Elternastes (0 = Preset-Wert benutzen).
static void ez_grow(int lvl, const Sec *base, const EzPreset *P, float unit,
                    float seed, int tier, float btone, int secOv, int segOv){
  int S = (secOv > 0) ? secOv : P->sections[lvl];
  int G = (segOv > 0) ? segOv : P->segments[lvl];
  // Detail wie bei EZ-Tree: NUR die Vernetzung aendert sich, nie die Struktur.
  // sectionStride = jeden N-ten Ring vernetzen (erster/letzter bleibt),
  // segmentFactor = radiale Segmente. Ganzz Leveln wegzuwerfen (unser altes
  // tier-System) erzeugt den Sprung in der Aststruktur, den der Nutzer sah.
  int stride = (tier >= 3) ? 1 : (tier == 2 ? treeT2Stride : 6);
  if(tier == 2) G = (G * treeT2Gnum) / treeT2Gden;   // DEMO_T2 (Sweep-Bank)
  else if(tier == 0) G = (G * 2) / 5;
  if(G < 3) G = 3;
  if(G > 8) G = 8;
  // Kinderzahl: in ALLEN Stufen gleich (deren Regel: detail ändert nur die
  // Vernetzung). Der Wald hat 646 Baeume, ihre Presets sind fuer wenige
  // gerechnet — die Dichte deshalb ueber einen GLOBALen Faktor (DEMO_KIDPCT),
  // der jede Stufe gleichmaessig trifft, statt ueber die Stufe.
  int kidPct = ezKidPct;

  // Kronenlevel-Laengen sind extra skalierbar: ihre Presets sind schlanker als
  // unsere Kronen proportioniert (Breite/Hoehe ~0,4 statt ~0,85).
  float len = P->length[lvl] * unit * (lvl == 0 ? 1.0f : treeEzSpread);
  float sl = len / (float)S;
  Sec sec[13];
  sec[0] = *base;
  int keep[13]; int nkeep = 0;          // vernetzte Ringe (sectionStride)
  keep[nkeep++] = 0;

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
    if(i % stride == 0 || i == S) keep[nkeep++] = i;
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
  // Ansatzpunkte fuellen IMMER auf den vernetzten Ringen liegen: die sichtbare
  // Roehre ist die Sehne zwischen keep-Ringen, der Abschnittspfad dazwischen ist
  // durch Gnarliness gebogen. Wuerden Kinder/Laub auf den gebogenen Pfad gesetzt,
  // wuerden Zweige in der Luft haengen (gemeldet in Runde 24).
  #define EZSEG(tt) do{ \
    float _ts = (tt) * (float)S; int _k = 0; \
    while(_k < nkeep - 2 && (float)keep[_k+1] < _ts) _k++; \
    si = keep[_k]; si2 = keep[_k+1]; \
    al = (float)si2 > (float)si ? (_ts - (float)si) / ((float)si2 - (float)si) : 0.0f; \
    if(al < 0.0f) al = 0.0f; \
    if(al > 1.0f) al = 1.0f; \
  }while(0)

  int maxKid = P->levels;               // Struktur in ALLEN Stufen gleich
  Sec kids[96]; int nkid = 0;
  if(lvl < maxKid){
    int cnt = (P->children[lvl] * kidPct) / 100;
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
      int si, si2; float al; EZSEG(t);
      Sec cb;
      for(int k = 0; k < 3; k++)
        cb.rg.p[k] = sec[si].rg.p[k] + (sec[si2].rg.p[k] - sec[si].rg.p[k]) * al;
      // Kinder-Radius IMMER aus dem Elternast abgeleitet
      float rp = sec[si].rg.r + (sec[si2].rg.r - sec[si].rg.r) * al;
      cb.rg.r = rp * P->radius[lvl + 1];
      // Elternrahmen bei t (u/v interpolieren, dann orthonormieren)
      float ux = sec[si].rg.u[0] + (sec[si2].rg.u[0] - sec[si].rg.u[0]) * al;
      float uy = sec[si].rg.u[1] + (sec[si2].rg.u[1] - sec[si].rg.u[1]) * al;
      float uz = sec[si].rg.u[2] + (sec[si2].rg.u[2] - sec[si].rg.u[2]) * al;
      float px = sec[si].d[0] + (sec[si2].d[0] - sec[si].d[0]) * al;
      float py = sec[si].d[1] + (sec[si2].d[1] - sec[si].d[1]) * al;
      float pz = sec[si].d[2] + (sec[si2].d[2] - sec[si].d[2]) * al;
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

  // ---- Roehren senden: nur an den gehaltenen Ringen (sectionStride) ----
  // tier 0 (d > 20 x Wuchshoehe): Aeste sind dort 1..2 Pixel gross und liegen
  // voll in den Laubkarten. Gemessen: 224 tier-0-Baeume kosten 0,78 Mio der
  // 4,08 Mio Geruest-Vertices, ohne sichtbaren Beitrag. Deshalb bleibt nur der
  // Stamm stehen (tragende Silhouette), die Zweigroehre entfaellt.
  int meshBranch = !(tier == 0 && lvl > 0);
  float tone = btone * (0.86f + 0.05f * (float)lvl);
  float cr = (0.30f - 0.03f * (float)lvl) * tone;
  float cg = (0.24f + 0.03f * (float)lvl) * tone;
  float cb2 = (0.15f - 0.02f * (float)lvl) * tone;
  float sw = 0.20f + 0.30f * (float)lvl;
  if(meshBranch){
    int prev = 0;
    for(;;){
      int cur = prev + stride; if(cur > S) cur = S;
      emit_tube(&sec[prev].rg, &sec[cur].rg, G, sw, sw, cr, cg, cb2, 0.9f,
                (float)cur * sl);
      prev = cur;
      if(cur == S) break;
    }
  }
  // Ast-Enden verschliessen. Nicht-terminale Aeste enden mit vollem Radius —
  // deren taper-Regel laesst bei Eiche 27 % des Stammradius offen, bei Astlevel 1
  // sogar 58 %. Ohne Abschluss sieht man in das offene Rohr hinein (Nutzer-
  // befund "abgebrochene Aeste", "abgeschnittener Stamm oben in der Krone").
  // Terminale Aeste brauchen keinen: deren letzter Abschnitt geht auf 0,001 zu.
  if(lvl < maxKid && meshBranch){
    int sides = G >= 3 ? G : 3;
    emit_cap(&sec[S].rg, sides, sw, cr, cg, cb2, 0.9f, (float)S * sl);
  }

  // DER FEHLENDE MECHANISMUS (Runde 25): deren #growBranch schiebt fuer jeden
  // Nicht-Terminalast einen Fortsetzungszweig an die Spitze — gleiche Richtung,
  // Radius = Spitzenradius, Laenge length[lvl+1], Section-/Segmentzahl wie der
  // Elternast. Referenz oak_medium (node /tmp/ezcmp): 183 Aeste, Stamm 37,2 von
  // 65,6 Einheiten Baumhoehe. Ohne diesen Zweig endet bei uns der Stamm bei 55 %
  // und die Krone wachst nur 18 statt 30 Einheiten ueber die Stamm spitze — das
  // ist der "abgeschnittene Stamm" und das "zu wenig Begruente".
  if(lvl < maxKid){
    Sec cont = sec[S];
    ez_grow(lvl + 1, &cont, P, unit, seed + 91.7f + (float)lvl * 5.1f,
            tier, btone, S, G);
  }

  if(lvl >= maxKid){
    // Terminalast: Laub geschichtet entlang des Astes (deren Laub-Schema).
    // Laubpositionen werden in ALLEN Stufen gesammelt — die Atlas-Karten sind
    // bei fern die einzige Laubdarstellung. Nur die Einzelblaetter entfallen.
    int lc = (int)P->leafCount;
    // Laub in ALLEN Stufen (deren Regel: leafStride/leafScale, nie "kein Laub").
    // Einzelblaetter gibt es, solange ezWantLeaves (Entfernung bis 1,1 lodR),
    // Karten uebernehmen komplementaer ab 0,9 lodR.
    int leafStride = 1;
    const float CARDSCALE = ezCardScale;   // deren leafScale: 1,25 (LOD1) / 1,3 (LOD2)
    if(lc < 1) return;
    float lsz = P->leafSize * unit;
    if(lsz > ezLszMax) ezLszMax = lsz;
    float lcr = ezLeafR * (0.85f + 0.30f * TH(700)),
          lcgg = ezLeafG * (0.85f + 0.30f * TH(701)),
          lcb  = ezLeafB * (0.85f + 0.30f * TH(702));
    for(int i = 0; i < lc; i++){
      float st = (1.0f - P->leafStart) / (float)lc;
      float t = P->leafStart + (i + TH(400 + i)) * st;
      if(t > 1.0f) t = 1.0f;
      int si, si2; float al; EZSEG(t);
      float cx = sec[si].rg.p[0] + (sec[si2].rg.p[0] - sec[si].rg.p[0]) * al;
      float cy = sec[si].rg.p[1] + (sec[si2].rg.p[1] - sec[si].rg.p[1]) * al;
      float cz = sec[si].rg.p[2] + (sec[si2].rg.p[2] - sec[si].rg.p[2]) * al;
      if(ezNSprig >= EZ_SPRIGCAP) treeCapSprig++;
      if(ezNSprig < EZ_SPRIGCAP){       // Karte fuer die Distanzstufe
        ezSx[ezNSprig] = cx; ezSy[ezNSprig] = cy; ezSz[ezNSprig] = cz;
        // Groessenstreuung wie bei ihnen: leafSize * (1 +/- sizeVariance=0,7).
        // Der alte Faktor (1,6..2,5) war auf 22 Riesenquads pro Baum getunt; mit
        // 648 Karten pro Baum waere die Krone damit ueberfuellt.
        ezSsize[ezNSprig] = lsz * (0.30f + 1.40f * TH(430 + i)) * CARDSCALE;
        ezNSprig++;
      }
      if(ezWantLeaves && (i % leafStride) == 0){   // Einzelblaetter (NAHstufe)
        float rr = lsz * (0.30f + 0.25f * TH(430 + i));
        leaf_cluster(cx, cy, cz, rr * ezLeafScale, (int)(5.0f * ezLeafPer),
                     lsz * 0.45f * ezLeafScale, 0.85f, 0.55f,
                     seed + (float)i * 7.3f, 500 + i * 3, lcr, lcgg, lcb);
      }
    }
    return;
  }

  if(lvl == 0){                       // Messbank: Stammradien pro Abschnitt
    ezDbgNSec = S;
    for(int i = 0; i <= S && i < 13; i++) ezDbgR[i] = sec[i].rg.r;
    if(treeTierLog)
      printf("    Stamm: %d Abschnitte, Spitze (%.2f, %.2f, %.2f) Radius %.3f m, "
             "Basisradius %.3f m\n", S,
             sec[S].rg.p[0], sec[S].rg.p[1], sec[S].rg.p[2], sec[S].rg.r, base->rg.r);
  }
  for(int i = 0; i < nkid; i++){
    if(treeTierLog && lvl == 0)
      printf("    Ast-Ansatz lvl1 %d: t %.2f  Ansatz (%.2f, %.2f, %.2f)\n", i,
             (float)i / (float)(nkid > 1 ? nkid - 1 : 1),
             kids[i].rg.p[0], kids[i].rg.p[1], kids[i].rg.p[2]);
    ez_grow(lvl + 1, &kids[i], P, unit,
            seed + (float)lvl * 17.0f + (float)i * 3.13f, tier, btone, 0, 0);
  }
}

static void gen_tree_ez(float bx, float by, float bz, float s, float seed, int tier,
                        float d, float R1){
  (void)d; (void)R1;   // frueher steuerte die Distanz noch, ob Karten gebaut werden
  int pidx = (int)(TH(0) * (float)(EZ_NPRESETS - 0.001f));
  if(treeEzPreset >= 0 && treeEzPreset < EZ_NPRESETS) pidx = treeEzPreset;
  // Impostor-Eimer: Art x Form. 4 Presets x 16 Formen = 64 Kacheln.
  ezBucket = pidx * (IMP_TILES / EZ_NPRESETS) +
             ((int)(TH(903) * (float)(IMP_TILES / EZ_NPRESETS))) % (IMP_TILES / EZ_NPRESETS);
  const EzPreset *P = &EZ_PRESETS[pidx];
  float hf = 0.80f + TH(1) * 2.20f;
  // dieselbe Groessenlogik wie Generator A: Kronenmittelpunkt-Hoehe ueber Basis
  float treeH = 1.5f * hf * 1.15f * s;
  float unit = treeH / P->length[0];
  float btone = 0.75f + 0.55f * TH(2);
  // eigene Blattfarbe pro Baum (wie im alternativen Generator) — sie sitzt auch
  // in der Karten-Instanz, damit die Distanzstufe dieselbe Krone zeigt wie nah
  float hueE  = ezHueMin + TH(800) * (ezHueMax - ezHueMin);
  float lsatE = ezSatMin + TH(801) * (ezSatMax - ezSatMin);
  float lvalE = ezValMin + TH(802) * (ezValMax - ezValMin);
  hsv2rgb(hueE, lsatE, lvalE, &ezLeafR, &ezLeafG, &ezLeafB);
  Sec root;
  root.rg.p[0] = bx; root.rg.p[1] = by; root.rg.p[2] = bz;
  root.rg.r = P->radius[0] * unit;
  root.d[0] = 0.0f; root.d[1] = 1.0f; root.d[2] = 0.0f;
  ring_basis(&root.rg, 0.0f, 1.0f, 0.0f);
  ezNSprig = 0;
  ezLszMax = 0.0f;
  // Karten-Groesse gleicht die Ausduennung aus (div 2 -> 1,25, div 8 -> 2,20);
  // muss VOR ez_grow stehen, die Groesse sitzt in der Sprig-Liste.
  ezCardScale = (tier >= 2) ? treeCardScaleN : (tier == 1 ? treeCardScale0 : 2.20f);
  ezDbgName = P->name; ezDbgUnit = unit; ezDbgR0 = root.rg.r; ezDbgNSec = 0;  ezDbgS = s; ezDbgHf = hf;
  ez_grow(0, &root, P, unit, seed, tier, btone, 0, 0);
  // Karten (Distanzstufe): ab 0,9 lodR uebernehmen sie das Laub, im Band um
  // lodR existieren beide und der Shader blendet sie um lodR +/-10 % um.
  // Dichte/Groesse wie deren LOD1/LOD2: jeder zweite / vierte Laubansatz.
  // Nach oben gibt es keine Kante mehr — die Krone endet sonst kahl bei R2
  // (gemessen: ab 1,1 R2 war vorher GAR kein Laub mehr da). Das Billboard
  // (TODO 23) eretzt diese Stufe spaeter, nicht eine Luecke.
  // Karten gibt es in JEDER Stufe — auch bei einem beim Bau nahen Baum. Die
  // Stufe wird nur alle 12 m Kameraweg neu gebaut (main.c), ein Baum kann also
  // bis ~17 m veraltet sein. Ohne Karten verschwindet beim Entfernen das Laub:
  // der Shader blendet die Einzelblaetter weg, nichts uebernimmt (gemessen: 79
  // von 640 Baeumen waren beim Bau tier 3 -> 0 Karten). Zusatzkosten klein: nur
  // tier-3-Baeume bekommen sie, jeder vierte Laubansatz (79 x ~82 Karten = 39k
  // von 2,66 Mio Geruest-Vertices).
  // Deckung: tier 0 bekam bisher nur ezNSprig/4 Karten -> die ferne Krone deckt
  // 34 % weniger Pixel ab als die nahe (gemessen 1060 vs 1618 Kronenpixel).
  // Gleiche Kartenanzahl wie tier 2 gleicht das aus; die eingesparten
  // Zweigroehren (meshBranch) finanzieren das.
  // Karten sind die BASISstufe der Krone: jeder Baum in jeder Stufe hat sie,
  // der Shader zeichnet sie ohne Entfernungsschwelle. Einzelblaetter legen sich
  // als Detailschicht darueber, das Billboard ersetzt sie in der Ferne. Eine
  // Stufe ohne Karten (frueher tier 0) laesst beim Annaehern nichts einblenden.
  // Weniger Karten in der groebsten Stufe, aber grosse: Deckung bleibt gleich.
  // Karten werden bis 0,7 R1 gebaut, obwohl der Shader sie erst ab 0,9 R1
  // einblendet: der Build ist bis 12 m Kameraweg alt, ohne Vorrat waere beim
  // Annaehern nichts zum Einblenden da (derselbe Fehler, den die Einzelblaetter
  // mit 1,3 R1 Puffer hatten).
  int cardDiv = (tier >= 2) ? treeCardDivN : (tier == 1 ? treeCardDiv0 : (treeBB0Cards > 0 ? treeBB0Cards : 8));
  int wantCards = ezNSprig / cardDiv;
  // BBox wird fuer JEDEN Baum bestimmt (die Aufnahme-Rahmung braucht sie auch
  // fuer tier-3-Repraesentanten, die selbst kein Billboard bekommen)
  gBbWant = (treeBillboard0 && tier <= 2) ? 1 : 0;
  {
    // Das Billboard liegt auch in Stufe 2 im Puffer: der Shader blendet es ab
    // 0,7 R2 ein und die Karten bis 1,1 R2 aus — der Uebergang laeuft mit der
    // Entfernung, nicht mit dem Build (kein Dichtesprung an der Schaltweite).
    emit_crown_billboard(seed, bx, by, bz, treeH);
    // DEMO_BB0CARDS=<div>: zusaetzliche Einzelkarten unter dem Billboard der
    // groebsten Stufe (0 = nur Billboard). Mit div 8 bekommt die ferne Krone
    // wieder etwas Parallax-Struktur, fuer 364 Baeume ~1/3 der alten Kartenzahl.
  }
  if(wantCards > 0) emit_sprig_quads(seed, wantCards);
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

// Atlas-Vorlage: EIN Blattbuschel (Rebe), nicht eine ganze Krone.
// Eine Distanz-Karte steht bei uns fuer genau einen Laubansatz des Baumes —
// genau wie bei EZ-Tree, deren "leaf" eine Texturkarte mit mehreren Blaettern
// ist. Vorher lag in der Kachel eine ganze Referenzkrone (40 Klumpen a 50
// Blaetter, 2,4 m Durchmesser), die dann auf einer 0,45-m-Karte landetet: die
// Karte zeigte eine ganze Krone, und das Nah-LOD zeigte Einzelblaetter.
// Die Farbe ist absichtlich NEUTRAL: die Blattfarbe DIESES Baumes steckt in der
// Karten-Instanz und wird im Kronen-Shader multipliziert.
int foliage_patch(float *out, int cap, float seed, float s){
  #define THP(i) c_hash12(seed * 0.731f + (i) * 13.7f, seed * 1.173f + (i) * 7.9f)
  int n = 0;
  int cnt = 9 + (int)(THP(0) * 4.9f);              // 9..13 Blaetter pro Buschel
  for(int i = 0; i < cnt && n < cap; i++){
    float h1 = THP(10 + i * 3), h2 = THP(11 + i * 3), h3 = THP(12 + i * 3);
    // Richtungen gleichmaessig ueber die Kugel (goldener Winkel, gleichmaessiges z)
    float th = (float)i * GOLD;
    float nz = 1.0f - 2.0f * ((float)i + 0.5f) / (float)cnt;
    float sr = sqrtf(fmaxf(0.0f, 1.0f - nz * nz));
    float rr = s * (0.15f + 0.11f * h1);
    float *d = &out[n * LEAF_FLOATS];
    d[0] = sr * cosf(th) * rr;
    d[1] = nz * rr * 0.9f;
    d[2] = sr * sinf(th) * rr;
    d[3] = s * (0.19f + 0.12f * h2);               // Blatt = ~1/3 der Kartenbreite
    d[4] = h3 * 6.2832f;
    d[5] = 0.35f + h1 * 0.75f;                     // Neigung gegenueber dem Stiel
    d[6] = 0.38f + 0.22f * h2;                     // Seitenverhaeltnis (nie 1)
    d[7] = 1.0f; d[8] = 1.0f; d[9] = 1.0f;         // farbneutral
    d[10] = 1e9f;                                  // Atlas: nie ausduennen
    n++;
  }
  #undef THP
  return n;
}

typedef struct { float p[3]; float s; float d2; int ix, iz; float H; } TreeCand;

// Hysterese der Stufenwahl, wie bei EZ-Tree (0,05 als Bruchteil des Schalt-
// radius). Der Wald wird nur neu gebaut, wenn sich das Build-Zentrum um 12 m
// bewegt; ohne Gedaelnnis wechselt ein Baum beim selben Standort die Stufe, nur
// weil das Zentrum wandert — das war die "seltsame Hysterese" im Bild.
#define LODMEM 2048
static struct { int ix, iz; signed char tier; } lodMem[LODMEM];
static int lodGet(int ix, int iz){
  unsigned h = (unsigned)ix * 73856093u ^ (unsigned)iz * 19349663u;
  int k = (int)(h & (LODMEM - 1));
  return (lodMem[k].tier >= 0 && lodMem[k].ix == ix && lodMem[k].iz == iz)
       ? (int)lodMem[k].tier : -1;
}
static void lodSet(int ix, int iz, int tier){
  unsigned h = (unsigned)ix * 73856093u ^ (unsigned)iz * 19349663u;
  int k = (int)(h & (LODMEM - 1));
  lodMem[k].ix = ix; lodMem[k].iz = iz; lodMem[k].tier = (signed char)tier;
}

// ---- Build in Phasen (Worker-Thread + Doppel-Puffer) ----
// frueher: trees_build() erzeugte den ganzen Wald in EINEM Aufruf = 350 ms
// Blockierung im Frame (gemessen, Nutzerbefund "stocken").
#define TREE_CAND_CAP 4096
static TreeCand treeCand[TREE_CAND_CAP];
static int treeNCand = 0, treeCandPos = 0;

void trees_build_begin(float tx, float tz){
  nTreeVerts = 0; nCrownVerts = 0; nWireVerts = 0; nTrees = 0; nLeafInst = 0;
  for(int q = 0; q < IMP_TILES; q++){ impRepD[q].valid = 0; impRepD[q].nTree = 0; }
  for(int q = 0; q < IMP_TILES; q++) impNTree[q] = 0;
  #define CAND_CAP TREE_CAND_CAP
  TreeCand *cand = treeCand;
  int nc = 0;
  treeCandPos = 0;
  const int RANGE = treeBuildRange;   // Zellen-Radius um das Kameraziel (20 x 9 m
                          // groesseren Sichtweite: march bis 400m)
  // Referenzpunkt fuer Detailstufe und Budget: die ECHTE Kamera. Vorher das
  // Build-Zentrum = camTgt, das haengt in Fly-Kamera 10 m in Blickrichtung vor
  // der Kamera -> ein Baum vor der Nase wurde so eingestuft, als waere man 10 m
  // naeher, einer hinter einem als 10 m ferner.
  float rx = treeLodRefValid ? treeLodRef[0] : tx;
  float rz = treeLodRefValid ? treeLodRef[2] : tz;
  int ix0 = (int)floorf((tx - RANGE * TREE_G) / TREE_G);
  int ix1 = (int)floorf((tx + RANGE * TREE_G) / TREE_G);
  int iz0 = (int)floorf((tz - RANGE * TREE_G) / TREE_G);
  int iz1 = (int)floorf((tz + RANGE * TREE_G) / TREE_G);
  for(int iz = iz0; iz <= iz1; iz++)
  for(int ix = ix0; ix <= ix1; ix++){
    float tp[3], s, hf;
    if(!tree_cell(ix, iz, tp, &s, &hf)) continue;
    if(nc < CAND_CAP){
      cand[nc].p[0] = tp[0]; cand[nc].p[1] = tp[1]; cand[nc].p[2] = tp[2];
      cand[nc].s = s; cand[nc].ix = ix; cand[nc].iz = iz;
      // Wuchshoehe ueber Grund — dieselbe Formel, die auch tree_jump() rahmt.
      cand[nc].H = s * 1.5f * hf * 1.15f;
      float ddx = tp[0] - rx, ddz = tp[2] - rz;
      cand[nc].d2 = ddx * ddx + ddz * ddz;
      nc++;
    }
  }
  // NAHE vor FERN erzeugen: das Blatt-Budget (LEAF_CAP) ist begrenzt. In
  // Zell-Reihenfolge hatten ferne Baeume es schon aufgebraucht, bevor der
  // Baum direkt vor der Kamera dran war (gemessen: 300k Instanzen verpufft).
  // Sortierung steht in trees_sort_candidates() — sie laeuft zwischen begin()
  // und dem ersten step(), damit step() in hockern weiterarbeiten kann.
  // DEMO_ONETREE: genau den Baum bauen, den die Debug-Kamera gerahmt hat
  // (tree_jump). Vorher war es der naechste am Build-Zentrum = camTgt, ab ~24 m
  // Abstand also systematisch ein ANDERER Baum -> jede Abstandsmessung wertlos.
  if(treeOneShot && nc > 1){
    int best = 0;
    if(treeFocusValid){
      float bd = 1e30f;
      for(int i = 0; i < nc; i++){
        float ddx = cand[i].p[0] - treeFocus[0], ddz = cand[i].p[2] - treeFocus[2];
        if(ddx * ddx + ddz * ddz < bd){ bd = ddx * ddx + ddz * ddz; best = i; }
      }
    }
    TreeCand t = cand[0]; cand[0] = cand[best]; cand[best] = t;
    nc = 1;
  }
  treeNCand = nc;
}

// erzeugt hoechstens maxTrees Baeume; 1 = Build fertig
int trees_build_step(int maxTrees){
  TreeCand *cand = treeCand;
  int end = treeCandPos + maxTrees; if(end > treeNCand) end = treeNCand;
  for(int i = treeCandPos; i < end; i++){
    float d = sqrtf(cand[i].d2), H = cand[i].H;
    // Schaltweiten: R1 = wo die EINZELBLAETTER aufhoeren zu decken, R2 = wo das
    // Ganzbaum-Billboard uebernimmt. 2026-10-07 auf 10x/20x Wuchshoehe gesetzt,
    // dann gemessen (ein Baum, Laubpixel ueber der Entfernung, DEMO_TREEDIST):
    //   10 m 46470 | 25 m 20569 | 50 m 13896 | 60 m 7280 | 86 m 2934  (kahl!)
    //   90 m 12828 | 108 m 20231  <- erst hier kamen die Karten dazu
    // Ein 6-cm-Blatt ist ab ~50 m unter einem Pixel, die Einzelblaetter decken
    // die Krone also nur bis etwa 5x Wuchshoehe. Die Luecke dazwischen war der
    // "verschwindende Blattwald". Deshalb jetzt 5x und 15x, absolut begrenzt.
    float R1 = 5.0f * H, R2 = 15.0f * H;
    if(R1 < 20.0f) R1 = 20.0f; else if(R1 > 60.0f) R1 = 60.0f;
    if(R2 < 40.0f) R2 = 40.0f; else if(R2 > 120.0f) R2 = 120.0f;
    // DEMO_LODRSCALE: Schaltweiten skalieren — Messbank, um an EINEM Standort
    // beide Repraesentationen (Einzelblaetter / Karten) vergleichend zu sehen.
    if(treeLodScale > 0.0f){ R1 *= treeLodScale; R2 *= treeLodScale; }
    int tier;
    if(treeForceTier >= 0) tier = treeForceTier;
    else {
      int prev = lodGet(cand[i].ix, cand[i].iz);
      if(prev == 3)      tier = (d < R1 * 1.05f) ? 3 : (d < R2 ? 2 : 0);
      else if(prev == 2) tier = (d < R1 * 0.95f) ? 3 : (d < R2 * 1.05f ? 2 : 0);
      else if(prev == 0) tier = (d < R2 * 0.95f) ? 2 : 0;
      else               tier = (d < R1) ? 3 : (d < R2 ? 2 : 0);
      lodSet(cand[i].ix, cand[i].iz, tier);
    }
    // Laub-Entscheidung haengt an der Entfernung, nicht an der Build-Stufe.
    // Der Shader blendet die Einzelblaetter im Band 0,9..1,1 R1 aus. Damit in
    // diesem Band IMMER beide Schichten im Puffer liegen (das Build entscheidet
    // nur, was existiert, und ist bis 12 m Kameraweg alt), werden Blaetter bis
    // 1,3 R1 gebaut. Fehlte der Puffer, blendete nichts ein: gemessen 2934
    // Laubpixel auf 86 m gegen 12828 auf 90 m fuer denselben Baum.
    gLodR = R1;
    gLodR2 = R2;
    ezWantLeaves = (d <= R1 * 1.30f);
    // Budget: passt ein BAUM nicht mehr in die Puffer, wird er uebersprungen —
    // und nicht mitten im Ast abgebrochen. Ein abgebrochener Baum sieht aus wie
    // ein abgesaegter Stamm ohne Laub (Nutzerbefund; gemessen: 54 Cap-Abbrueche,
    // Karten-Puffer 1 800 000/1 800 000 voll, Geruest 2 663 286/2 700 000).
    // Baeume werden von nah nach fern gebaut, es fehlen also nur die fernsten.
    // Groessen pro Baum GEMESSEN (DEMO_TREELOG, drei Kamerapositionen):
    //   tier 3: bis 30 096 Geruest-Vertices, 1 230 Karten, 12 300 Blatt-Instanzen
    //   tier 2: bis  8 016 Vertices, 1 230 Karten, 12 300 Instanzen
    //   tier 0: bis  2 988 Vertices,  615 Karten
    // Passt der Baum nicht ins Budget, wird er HERUNTERGESTUFT statt weggelassen
    // (Selbsttest tools/selftest.sh hat es gefunden: bei DEMO_LODRSCALE=2 fehlten
    // 296 Baeume komplett). Reihenfolge: erst die Detailstufe auf die groebste
    // (Stamm + ein Ganzbaum-Billboard = 45 Vertices), dann die Einzelblaetter
    // weg, erst dann der ganze Baum. Baeume werden von nah nach fern gebaut,
    // getroffen hat es also immer die fernsten.
    int tUse = tier, leavesWanted = ezWantLeaves, fits = 0;
    for(;;){
      int needV = (tUse >= 3) ? 31000 : (tUse == 2 ? 8500 : 200);
      int needC = (tUse >= 1) ? 1400 : 400;   // tier 0: Billboard + ~250 Karten
      int needL = leavesWanted ? 12500 : 0;
      if(nTreeVerts + needV <= TREE_VERT_CAP && nCrownVerts + needC <= CROWN_CAP &&
         nLeafInst + needL <= LEAF_CAP){ fits = 1; break; }
      if(tUse > 0){ tUse = 0; treeDowngraded++; continue; }
      if(leavesWanted){ leavesWanted = 0; continue; }
      break;
    }
    if(!fits){ treeSkipped++; continue; }
    tier = tUse; ezWantLeaves = leavesWanted;
    int leafBefore = nLeafInst, quadBefore = nCrownVerts, vertBefore = nTreeVerts;
    float tseed = cand[i].p[0] * 12.9898f + cand[i].p[2] * 78.233f;
    treeCapTree = treeCapCrown = treeCapLeaf = treeCapSprig = 0;
    if(treeGenMode) gen_tree_ez(cand[i].p[0], cand[i].p[1], cand[i].p[2], cand[i].s, tseed, tier, d, R1);
    else            gen_tree (cand[i].p[0], cand[i].p[1], cand[i].p[2], cand[i].s, tseed, tier);
    // Sicherheitsnetz: die Budget-Schaetzung oben ist pro Baum nur geschaetzt.
    // Hat der Baum trotzdem ein Cap gerissen, wird er KOMPLETT zurueckgerollt —
    // ein Baum, der mitten im Ast endet, zeigt eine offene Ringkante (eine
    // Schnittflaeche) und hoert sich fuer den Nutzer "abgebrochener Baum" an.
    // Die Puffer werden pro Baum nur angehaengt, zurueckrollen ist also billig.
    // Repraesentant fuer den Impostor-Eimer: der NAHSTE Baum der Sorte (wir
    // bauen nah vor fern), weil von ihm Geruest UND Karten im Puffer liegen.
    // Repraesentant: der NAHSTE Baum der Sorte MIT DICHTEN Karten. Ferne Baeume
    // (tier 0) bekommen nur jede achte Position in Groesse 2,2 — als Aufnahme
    // in eine 128-px-Kachel gibt das wenige grosse Blatt-Lappen statt Laubmasse
    // (im Atlas deutlich gesehen). Also tier 2/3 bevorzugen, einer von tier 0/1
    // nur als Ausfall.
    // "dicht" = Karten UND Einzelblaetter. Ein Repraesentant ohne Einzelblaetter
    // (tier 2 jenseits 1,3 R1) gibt in der Kachel einen Stapel grosser Karten-
    // Lappen statt einer feinen Krone (im Atlas gesehen).
    int dense = (tier >= 2) && (nLeafInst > leafBefore);
    if(ezBucket >= 0 && ezBucket < IMP_TILES) impNTree[ezBucket]++;
    if(ezBucket >= 0 && ezBucket < IMP_TILES &&
       (!impRepD[ezBucket].valid || (dense && !impRepD[ezBucket].dense)) &&
       nTreeVerts > vertBefore && nCrownVerts > quadBefore){
      struct ImpRep *ir = &impRepD[ezBucket];
      ir->valid = 1; ir->dense = dense; ir->tier = tier;
      ir->vStart = vertBefore; ir->vCount = nTreeVerts - vertBefore;
      ir->cStart = quadBefore; ir->cCount = nCrownVerts - quadBefore;
      ir->lStart = leafBefore; ir->lCount = nLeafInst - leafBefore;
      ir->nTree = impNTree[ezBucket];
      ir->cx = cand[i].p[0]; ir->cy = cand[i].p[1]; ir->cz = cand[i].p[2];
      ir->rad = (gBbSize > 0.5f ? gBbSize : H) * 0.5f;
      ir->hasBb = (gBbSize > 0.5f) ? 1 : 0;
      if(ir->hasBb){ ir->cx = gBbCen[0]; ir->cy = gBbCen[1]; ir->cz = gBbCen[2]; }
    }
    if(treeCapTree || treeCapCrown || treeCapLeaf || treeCapSprig){
      nTreeVerts = vertBefore; nCrownVerts = quadBefore; nLeafInst = leafBefore;
      treeSkipped++;
      continue;
    }
    if(treeTierLog)
      printf("  Baum (%.1f,%.1f) H %.1f m  d %.1f m  R1 %.0f R2 %.0f  tier %d  "
             "Blaetter %d  Karten %d  Geruest %d  Farbe (%.2f,%.2f,%.2f)\n",
             cand[i].p[0], cand[i].p[2], H, d, R1, R2, tier,
             nLeafInst - leafBefore, nCrownVerts - quadBefore, nTreeVerts - vertBefore,
             ezLeafR, ezLeafG, ezLeafB);
    nTrees++;
  }
  treeCandPos = end;
  return treeCandPos >= treeNCand;
}

// Sortierung NAHE vor FERN (nach dem Sammeln, vor dem ersten Schritt):
// das Blatt-Budget ist begrenzt — in Zell-Reihenfolge hatten ferne Baeume es
// schon aufgebraucht, bevor der Baum vor der Kamera dran war (gemessen: 300k
// Instanzen verpufft).
static void trees_sort_candidates(void){
  int nc = treeNCand;
  for(int i = 0; i < nc; i++)
    for(int j = i + 1; j < nc; j++)
      if(treeCand[j].d2 < treeCand[i].d2){ TreeCand t = treeCand[i]; treeCand[i] = treeCand[j]; treeCand[j] = t; }
}

// Tausch Schreib-/Zeichenseite. Von hier an sieht der Main-Thread den fertigen
// Build (und der Worker kann den naechsten in die jetzt freie Schreibseite bauen).
void trees_build_finish(void){
  pthread_mutex_lock(&treeMu);
  float *tp;
  tp = treeVerts;   treeVerts = treeVertsD;   treeVertsD = tp;
  tp = crownVerts;  crownVerts = crownVertsD; crownVertsD = tp;
  tp = leafInst;    leafInst = leafInstD;     leafInstD = tp;
  nTreeVertsD = nTreeVerts; nCrownVertsD = nCrownVerts;
  nLeafInstD = nLeafInst;   nTreesD = nTrees;
  // Ausfall-Aushilfe: ein Eimer kann nur ferne Baeume enthalten (tier 0 = nur
  // Stamm + jede achte Kartenposition in Groesse 2,2). Als 128-px-Aufgabe gibt
  // das wenige grosse Blatt-Lappen statt Laubmasse (im Atlas gesehen). Dann
  // nehmen wir den dichten Repraesentanten einer ANDEREN Form derselben Art —
  // auf 20-30 px Bildschirmgroesse ist der Formunterschied nicht sichtbar.
  for(int q = 0; q < IMP_TILES; q++){
    if(impRepD[q].valid && impRepD[q].dense) continue;
    for(int q2 = 0; q2 < IMP_TILES; q2++){
      if(q2 == q) continue;
      if(q2 / EZ_NPRESETS_BUCKETS != q / EZ_NPRESETS_BUCKETS) continue;
      if(!impRepD[q2].valid || !impRepD[q2].dense) continue;
      struct ImpRep t = impRepD[q];
      impRepD[q] = impRepD[q2];
      impRepD[q].valid = 1;
      (void)t;
      break;
    }
  }
  // die Kachel-Repraesentanten gehoeren zum Puffer, also mit tauschen
  for(int q = 0; q < IMP_TILES; q++) treeImp[q] = impRepD[q];
  treeBuildDone = 1; treeBuilding = 0;
  pthread_mutex_unlock(&treeMu);
}

// synchroner Komplettaufbau (erster Frame, Messlaeufe, Tests)
void trees_build(float tx, float tz){
  trees_build_begin(tx, tz);
  trees_sort_candidates();
  while(!trees_build_step(64)){ }
  trees_build_finish();
}

// ================= Build-Thread =================
// Muster wie beim Gras (grass_cpu.c): Main-Thread ruft trees_build_request(),
// der Worker baut zu Ende (kein Restart mitten drin), der Main-Thread holt den
// fertigen Build per trees_build_finished_snapshot() ab und laedt hoch — der
// GL-Kontext laeuft ausschliesslich im Main-Thread.
void trees_build_request(float tx, float tz, const float lodRef[3]){
  pthread_mutex_lock(&treeMu);
  treeReqTX = tx; treeReqTZ = tz;
  if(lodRef){ treeReqLod[0] = lodRef[0]; treeReqLod[1] = lodRef[1]; treeReqLod[2] = lodRef[2];
              treeReqLodValid = 1; }
  else treeReqLodValid = 0;
  treeReqPending = 1;
  pthread_cond_signal(&treeCv);
  pthread_mutex_unlock(&treeMu);
}

int trees_build_busy(void){
  pthread_mutex_lock(&treeMu);
  int b = treeBuilding || treeReqPending;
  pthread_mutex_unlock(&treeMu);
  return b;
}

// fertigen Build abholen (exakt einmalig); *tv == NULL wenn nichts fertig ist
void trees_build_finished_snapshot(float **tv, float **cv, float **lv,
                                   int *ntv, int *ncv, int *nli, int *ntrees){
  pthread_mutex_lock(&treeMu);
  if(treeBuildDone){
    *tv = treeVertsD; *cv = crownVertsD; *lv = leafInstD;
    *ntv = nTreeVertsD; *ncv = nCrownVertsD; *nli = nLeafInstD; *ntrees = nTreesD;
    treeBuildDone = 0;
  } else *tv = NULL;
  pthread_mutex_unlock(&treeMu);
}

static void *tree_worker(void *unused){
  (void)unused;
  for(;;){
    pthread_mutex_lock(&treeMu);
    while(!treeReqPending) pthread_cond_wait(&treeCv, &treeMu);
    treeReqPending = 0; treeBuilding = 1;
    float tx = treeReqTX, tz = treeReqTZ;
    if(treeReqLodValid){
      treeLodRef[0] = treeReqLod[0]; treeLodRef[1] = treeReqLod[1];
      treeLodRef[2] = treeReqLod[2]; treeLodRefValid = 1;
    }
    pthread_mutex_unlock(&treeMu);
    trees_build_begin(tx, tz);
    trees_sort_candidates();
    while(!trees_build_step(32)){ }
    trees_build_finish();
  }
  return 0;
}

int trees_thread_start(void){
  pthread_t t;
  if(pthread_create(&t, 0, tree_worker, 0) != 0) return 0;
  pthread_detach(t);
  return 1;
}
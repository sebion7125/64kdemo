// scene.h - gemeinsame Deklarationen fuer CPU-Geometrie-Module und main.c
// Die CPU-Module (terrain_cpu, trees_cpu, grass_cpu) sind reines C ohne GL:
// sie fuellen die globalen Vertex-Buffer, main.c macht den GL-Upload.
#ifndef SCENE_H
#define SCENE_H

// Wasserstand (mit fragment.glsl abgestimmt)
#define WATER_Y 0.25f
// Near/Far der Mesh-Projektion (mit build_mvp in main.c + linearT in fragment.glsl)
#define CAM_NEAR 0.1f
#define CAM_FAR  600.0f

// ================= Hilfen =================
static inline float c_smoothstep(float e0, float e1, float x){
  float t = (x - e0) / (e1 - e0);
  if(t < 0.0f) t = 0.0f;
  if(t > 1.0f) t = 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

// ================= CPU-Terrain (1:1-Port von fragment.glsl) =================
float c_hash12(float px, float py);
void  c_hash22(float px, float py, float *rx, float *ry);
float c_vnoise(float px, float py);
float c_fbm(float px, float py);
float c_rfbm(float px, float py);
float c_terrainH(float px, float py);

// ================= Baeume (prozedural, Mesh + Billboard-Blattdach) =================
// Vertex-Layouts (muessten zu den Shadern passen):
//   treeVerts:  pos(3) nrm(3) col(3) sway(1)             = 10 floats
//   crownVerts: center(3) off(2) size rotY seed variant lodR col(3) = 13 floats
//   wireVerts:  pos(3)
#define TREE_FLOATS  13   // pos3, nrm3, col3, sway, u (um den Ring), v (Bogenlaenge),
                            // R2 (Schaltweite Ganzbaum-Billboard dieses Baums)
// Kronen-Karte: center(3) off(2) size rotY seed variant lodR col(3) = 13 floats.
// lodR = Radius, ab dem diese Karte die Einzelblaetter dieses Baumes eretzt
// (blendet um lodR +/- 10 % ein). col = Blattfarbe DIESES Baumes: der Atlas ist
// farbneutral aufgenommen, die Farbe kommt pro Instanz dazu.
#define CROWN_FLOATS 12   // pro KARTE: center(3) size rot seed variant lodR col(3) lodR2
// Einzelblaetter an nahen Baeumen (leaf.vert): iPos(3), iData(4: size, rotY,
// tilt, aspect), iCol(3), iLodR(1). Grundblatt ist ein statisches Quad.
// iLodR = Radius, ab dem die Karten dieses Baumes die Blaetter uebernehmen
// (keep = 1 - smoothstep(0,9*lodR, 1,1*lodR, d)) — Komplement zu lodR oben.
#define LEAF_FLOATS  11
#define LEAF_CAP     2400000  // Budget erhoeht statt Geometrie zu ueberspringen (Nutzerregel):
                            // Stress-Szenario DEMO_LODRSCALE=2 brauchte 1,60 Mio, 90 Baeume
                            // fielen raus. 2,4 Mio x 40 B x 2 Saetze = 192 MB
// Referenz-Krone fuer den Blatthorizont-Atlas (ferne Baeume sampeln diese
// Textur statt prozeduraler Flecken). Layout im Atlas: 8x8 Kacheln =
// 4 Kronen-Varianten x 16 Ansichts-Richtungen.
#define FOLIAGE_CAP  2100
#define WIRE_FLOATS  3

// Caps fuer den vollen Wald (646 Baeume im Umkreis von 180 m) bei der neuen
// Stufenlogik: volle Dichte bis 10x Wuchshoehe, Karten bis 20x, dahinter Karten
// mit Stride 4. Gemessen (/tmp/test_full.c, Region (0,0)): 2 555 406 Geruest-
// Vertices, 284 118 Karten, 577 320 Einzelblaetter; Bauzeit 0,20 s. Nahe Baeume
// werden zuerst gebaut, bei Cap-Uberlauf fehlt Geometrie also ganz hinten —
// nicht vor der Kamera.
#ifndef TREE_VERT_CAP
#define TREE_VERT_CAP 9500000   // Netz-Dichte ist jetzt entfernungsabhaengig bis
                                // 1,15 R2 (kein Sprung bei der Blende) — Stress
                                // (DEMO_LODRSCALE=2) braucht 7,8 Mio; bei 5,2 Mio
                                // fehlten 217 Baeume. CPU-Puffer stehen im BSS,
                                // bezahlt wird nur was beschrieben wird.
                            // uebersprungen. 5,2 Mio x 32 B x 2 Saetze = 333 MB
                                // Doppel-Puffer (Build-Thread) = 2x diese Groesse.
#endif
#ifndef CROWN_CAP
#define CROWN_CAP     900000    // Karten (instanziert); gemessen 318k im Stress-Szenario
#endif
#define WIRE_CAP      90000

// Doppel-Puffer: der Build-Thread schreibt in die ohne Suffix, gezeichnet wird
// aus den D-Puffern (Swap in trees_build_finish(), unter Mutex).
extern float *treeVerts, *crownVerts, *leafInst;
// ---- Ganzbaum-Impostoren (TODO 23, Nutzer: "den Baum als Ganzes in eine
// Textur rendern und daraus ein Billboard bauen") --------------------------
// 8x8 Kacheln a 128 px in einer 1024x1024-Textur. Eine Kachel = eine
// Arten-/Form-Variante (4 Presets x 16 Formen), NICHT pro Baum: ferne Baume
// sind 20-30 px gross, dort darf Art+Groesse gleich aussehen. Das spart genau
// das, was der Nutzer nennt: Fuellrate und Geometrie-Transformation.
// Gerendert wird die KARTEN-Stufe dieses Baumes (div 3, ~250 Quads + Stamm),
// nie das Einzelblatt — in 128 px waere es Matsch.
#define IMP_COLS   8
#define IMP_TILES  64
#define IMP_TILEPX 128
struct ImpRep {
  int valid;
  int dense;                     // Repraesentant hat dichte Karten (tier 2/3)
  int tier;                      // Stufe des Repraesentanten
  int hasBb;                     // BBox-Mitte/-groesse des Billboards bekannt
  int nTree;                     // Baeume dieses Eimers ueberhaupt
  int vStart, vCount;            // Geruest-Slice im Baum-Puffer
  int cStart, cCount;            // Karten-Slice (Instanzen)
  int lStart, lCount;            // Einzelblatt-Slice (nur Aufnahme, nicht pro Frame)
  float cx, cy, cz, rad;         // Baum-Position, Radius der Huellkugel
};
extern struct ImpRep treeImp[IMP_TILES];   // Zeichenseite (getauscht im Finish)

extern float *treeVertsD, *crownVertsD, *leafInstD;
extern int   nLeafInst;
extern float wireVerts[WIRE_CAP * WIRE_FLOATS];
extern int   nTreeVerts, nCrownVerts, nWireVerts, nTrees;      // Schreibseite
extern int   nTreeVertsD, nCrownVertsD, nLeafInstD, nTreesD;  // Zeichenseite

// Baut die Baum-Geometrie um (tx,tz) (Zellen-Grid wie der alte SDF-Platzierer).
void trees_build(float tx, float tz);
// Phasen fuer den Worker-Thread (Muster wie grass_build_begin/step/finish):
void trees_build_begin(float tx, float tz);
int  trees_build_step(int maxTrees);      // 1 = fertig
void trees_build_finish(void);            // Schreib-/Zeichenseite tauschen
void trees_build_request(float tx, float tz, const float lodRef[3]);
int  trees_build_busy(void);
void trees_build_finished_snapshot(float **tv, float **cv, float **lv,
                                   int *ntv, int *ncv, int *nli, int *ntrees);
int  trees_thread_start(void);
// Ein-Baum-Szene (DEMO_ONE): was der Build fuer DEN einen Baum entschieden hat
extern int   treeDbgTier, treeDbgVerts, treeDbgCrown, treeDbgLeaf, treeDbgBuilds;
extern float treeDbgR1, treeDbgR2, treeDbgDBuild, treeDbgH, treeDbgX, treeDbgZ, treeDbgY;
extern int   treeDbgBucket; extern float treeDbgRepX, treeDbgRepZ;
extern int   treeBuildRange;   // Zellen-Radius, in dem Baeume gebaut werden
extern int treeCardDiv0; extern float treeCardScale0;   // DEMO_CARDT0 (Sweep-Bank)
extern float ezLeafScale, ezLeafPer;   // DEMO_LEAFSC / DEMO_LEAFPER
extern int treeCardDivN; extern float treeCardScaleN;   // DEMO_CARDT2 (Basisstufe)
extern int treeBillboard0;   // tier 0 als Ganzbaum-Billboard (TODO 23)
extern int treeBB0Cards;     // Zusatzkarten darunter (DEMO_BB0CARDS)
extern int treeDowngraded;   // wegen Budget auf die groebste Stufe herabgestuft
extern int treeT2Stride, treeT2Gnum, treeT2Gden;   // DEMO_T2 (Sweep-Bank)
extern int treeOneShot;
extern int treeGenMode;      // DEMO_TREEGEN=alt|ez: welcher Baum-Generator baut
extern int   treeEzPreset;
extern float treeEzSpread;   // DEMO_EZSPREAD: Laengenfaktor der Kronenlevel
extern const char *ezDbgName; extern float ezDbgUnit, ezDbgR0;
extern float ezDbgR[13]; extern int ezDbgNSec;
extern float ezDbgS, ezDbgHf;
extern int treeForceTier;    // DEMO_TIER=0|2|3: Detailstufe erzwingen (-1 = automatisch)
extern int treeTierLog;
extern int treeCapTree, treeCapCrown, treeCapLeaf;
extern int treeSkipped;
extern int treeCapSprig;      // DEMO_TREELOG=1: Stufe je Baum auf der Console
extern float treeLodScale;   // DEMO_LODRSCALE: Schaltweiten skalieren (0 = aus)
extern int ezKidPct;         // DEMO_KIDPCT: Kinderzahl aller Stufen gleich duennen
extern float ezHueMin, ezHueMax, ezSatMin, ezSatMax, ezValMin, ezValMax;  // Blatt-Palette
// Referenzpunkt fuer die Detailstufe. main.c setzt das auf die ECHTE Kamera
// (camPos). Vorher war es das Build-Zentrum = camTgt, das haengt 10 m in
// Blickrichtung vor der Kamera -> die Stufe hing von der Blickrichtung ab.
extern float treeLodRef[3]; extern int treeLodRefValid;
// Der Baum, den die Debug-Kamera gerahmt hat (tree_jump). Mit DEMO_ONETREE wird
// genau dieser gebaut, nicht der naechste am Build-Zentrum.
extern float treeFocus[3]; extern int treeFocusValid;
// Naechsten Baum zu (x,z) — Debug (Taste F11). Liefert 1, wenn einer gefunden wurde.
int  tree_find_nearest(float x, float z, float *out, float *sizeOut, float *hfOut);
// Eine Referenz-Krone aus echten Einzelblaettern, zentriert um (0, 2.05*s, 0).
// Fuellt out (LEAF_FLOATS-Stride) und liefert die Anzahl. seed waehlt die Krone.
int  foliage_patch(float *out, int cap, float seed, float s);

// ================= Gras (instanzierte Halme) =================
//   grassInst: pos(3) hoehe(1) breite(1) phase(1) schattierung(1) yaw(1) lean(1) birth(1) death(1) b(1) = 12 floats
//   hoehe/breite = BASIS-Werte; die Distanz-Skalierung passiert im Shader (grass.vert)
//   grp = Index in die Fade-GRUPPEN-Tabelle (grassGrp). Birth- und Death-Zeit
//   sind Fakten PRO BUILD, nicht pro Halm (grass_build_finish setzte frueher für
//   alle Halme eines Builds dieselbe Zeit) -> die zwei Pro-Halm-Floats waren
//   redundant. Ein Build erzeugt eine Birth-Gruppe; sterbende Halme bekommen
//   eine Gruppe, die ihre bisherige Birth-Zeit uebernimmt und die Death-Zeit
//   dieses Builds traegt. Der Halm speichert nur noch den Gruppen-Index.
//   Folge: 12 -> 10 Floats (auch der fruehere b-Index d[11] entfaellt, die
//   Zell-Blöcke sind jetzt in stabiler b-Reihenfolge -> Intervall statt Maske).
// ---------- Schilf-Pass (TODO 26) ----------
// Eine Instanz = eine PFLANZE (Culm + 4 Blaetter + Kolben), nicht ein Quad.
#define REED_FLOATS 8      // pos(3) hfac wfac yaw phase variante
#define REED_CAP    90000  // Pflanzen im Grid um die Kamera
#define REED_PARTS  5      // 1 Culm + 4 Blaetter
#define REED_STATIONS 6    // Stationen pro Streifen
// Streifen mit je 4 entarteten Eckpunkten aneinandergeklebt
#define REED_TEMPLATE_VERTS (REED_PARTS * REED_STATIONS * 2 + (REED_PARTS - 1) * 4)
extern float reedInst[REED_CAP * REED_FLOATS];
extern int   nReedInst;
extern int   reedRange;      // Grid-Halbseite [m]
extern float reedGridM;      // Gelaende-Raster [m]
extern float reedPerCell;    // Pflanzen pro Rasterzelle
void reed_build(float cx, float cz, float dens);

#define GRASS_FLOATS 10
#define GRASS_NGRP   512   // Fade-Gruppen (birth,death) — siehe grass_cpu.c.
// Messung: bei Fade 1s sind max ~30 Gruppen referenziert, bei L=30s (Debug) ~250.
// 512 = 4 KB Uniform. MUSS zu NGRP in src/shaders/grass.vert passen!
#define GRASS_CAP    5000000  // 5M (war 1.5M): Dichte-Slider max 5.6x staubt bei MD=120 am CAP

extern float *grassInst;   // aktuell sichtbar (Zweipuffer, zeigt auf den fertigen Build)
extern int   nGrass;
// Fade-Gruppen als flaches vec2-Array [birth0,death0, birth1,death1, ...];
// death == 0 heisst "lebt". Main-Thread uploadet es pro Frame als uFadeGrp.
extern float grassGrp[GRASS_NGRP * 2];
extern float grassDensityScale;   // Dichte-Skala (0.2-8.0, Slider zur Laufzeit)
extern float grassHeightScale;    // Halmhoehe-Skala (0.3-3.0, Slider zur Laufzeit)
extern float grassMaxDist;        // max. Gras-Sichtweite (40-120m, Slider)
extern float gWorldTime;          // aktuelle Weltzeit (fuer Birth-Time der Halme)

// Inkrementeller Rebuild (Werkzeuge; der Generator laeuft im Build-Thread):
//   begin  startet einen neuen Build in den zweiten Buffer (alt bleibt sichtbar)
//   step   verarbeitet max. maxCells Zellen, liefert 1 wenn fertig
//   finish tauscht die Buffer (grassInst/nGrass zeigen auf den neuen Build)
void grass_build_begin(float cx, float cz, int full);
int  grass_build_step(int maxCells);
void grass_build_finish(void);

// Build-Thread (Generator in eigenem Thread, "latest wins" bei neuen Requests):
//   request      neues Build-Zentrum anfordern (Main-Thread)
//   snapshot     fertigen Build abholen: buf (==NULL wenn nichts), n (Main-Thread,
//                danach VBO-Upload mit buf/n)
//   thread_start Worker-Thread starten (einmalig, nach dem ersten synchronen Build)
void grass_build_request(float cx, float cz, int full);
void grass_build_finished_snapshot(float **buf, int *n);
int  grass_build_busy(void);   // 1 = Request wartet oder Worker generiert
void grass_debug_dump(void);   // DEBUG F12: Buffer-Zustand + erwartete Shader-Fade
void grassSetDensCut(float v); // DEBUG-Zweig: Dichte-Abschnitt (0 = keine Kante)
// Sortierte Zeichenreihenfolge (DEBUG-Vergleich, I-Taste): Zellen beim Build
// fern->nah entlang der Blickrichtung ablaufen lassen, Puffer ist dann global
// hinten->vorn sortiert. grassSortedOrderActive() sagt dem Render-Pass, dass er
// von Index 0 aufwaerts zeichnen muss.
void grassSetViewDir(float x, float z);
void grassSetSortOrder(int on);
int  grassSortedOrderActive(void);
extern float grassFadeDur;     // DEBUG-Zweig: Fade-Dauer CPU+Shader (L-Taste)
void grass_debug_set_point(float x, float z);   // DEBUG n: Region-Dump-Punkt
void grass_debug_region_dump(int compare, int slot);   // DEBUG n/m: Halme um den Punkt
int  grass_thread_start(void);

#endif
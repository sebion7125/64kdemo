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
//   crownVerts: center(3) off(2) size(1) rotY(1) seed(1) pad(1) = 9 floats
//   wireVerts:  pos(3)
#define TREE_FLOATS  10
#define CROWN_FLOATS 9
#define WIRE_FLOATS  3

#define TREE_VERT_CAP 700000
#define CROWN_CAP     90000
#define WIRE_CAP      90000

extern float treeVerts[TREE_VERT_CAP * TREE_FLOATS];
extern float crownVerts[CROWN_CAP * CROWN_FLOATS];
extern float wireVerts[WIRE_CAP * WIRE_FLOATS];
extern int   nTreeVerts, nCrownVerts, nWireVerts, nTrees;

// Baut die Baum-Geometrie um (tx,tz) (Zellen-Grid wie der alte SDF-Platzierer).
void trees_build(float tx, float tz);

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
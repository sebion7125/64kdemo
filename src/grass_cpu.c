// grass_cpu.c - prozedurales Gras (instanzierte Halme, reines C, kein GL).
//
// Dichte nimmt mit der Distanz zur Kamera ab (nah: dicht, fern: lichter).
// Halme werden mit der Distanz groesser (decken die Flaechen ab) ->
// einheitliche visuelle Dichte. Die Groessen-Skalierung passiert im Shader
// (grass.vert) kontinuierlich zur aktuellen Kameradistanz, die CPU speichert
// nur Basis-Hoehe/-Breite -> springt nicht beim Rebuild.
// Zell-Hashes aus Weltkoordinaten -> Gras bleibt stabil bei Patch-Rebuilds.
//
// Inkrementeller Rebuild (keine CPU-Spitze bei Kamera-Bewegung):
//   begin  startet einen neuen Build in den zweiten Buffer (alt bleibt sichtbar)
//   step   verarbeitet max. N Zellen pro Frame (liefert 1 wenn fertig)
//   finish tauscht die Buffer; main.c uploadet danach den VBO
//
// Halm-Daten (GRASS_FLOATS=10): pos(3) hoehe breite phase schattierung yaw lean birth
//   yaw  = Rotation der Halm-Ebene um Y (sichtbar aus allen Richtungen)
//   lean = Neigung der Spitze in yaw-Richtung (sichtbar von oben)
#include "scene.h"
#include <math.h>
#include <stdio.h>
#include <pthread.h>

static float gA[GRASS_CAP * GRASS_FLOATS];
static float gB[GRASS_CAP * GRASS_FLOATS];
float *grassInst = gA;   // aktuell sichtbar (VBO-Inhalt)
int nGrass = 0;

static float *buildBuf = gB;
static int nGrassNew = 0;
static int cellNext = 0, cellEnd = 0;
static int gix0 = 0, giz0 = 0, gnx = 0;
static float bcx = 0, bcz = 0;
// Alter Build-Zentrum (fuer Fade-in: nur Halme außerhalb des alten Bereichs sind "neu")
static float old_bcx = 0, old_bcz = 0;
static int hasOldBuild = 0;

// Build-Thread-Synchronisation (siehe grass_worker am Dateiende):
//   gReqPending = Request wartet, gBuildDone = Build fertig (Abholung),
//   gReqSeq = Request-Sequenz (neuer Request waehrend Build = Restart)
static pthread_mutex_t gMu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  gCv = PTHREAD_COND_INITIALIZER;
static int   gReqPending = 0, gBuildDone = 0;
static int   gReqSeq = 0;
static float gReqCX = 0, gReqCZ = 0;

#define GSTEP 0.30f
// Build-Radius = max. Gras-Sichtweite + 20m Margin (damit bei Kameradrehung
// alles in Sichtweite schon "da" ist, ohne Neubuild)
static float g_half(void){ return grassMaxDist + 20.0f; }

void grass_build_begin(float cx, float cz){
  // Altes Zentrum merken (fuer Fade-in: Halme im alten Bereich sind "alt").
  // hasOldBuild wird erst in grass_build_finish gesetzt (erster Build =
  // alle Halme sofort sichtbar, kein Vergleich gegen das (0,0)-Zentrum).
  if (hasOldBuild) { old_bcx = bcx; old_bcz = bcz; }
  bcx = cx; bcz = cz;
  nGrassNew = 0;
  buildBuf = (grassInst == gA) ? gB : gA;
  float half = g_half();
  gix0 = (int)floorf((cx - half) / GSTEP);
  giz0 = (int)floorf((cz - half) / GSTEP);
  int ix1 = (int)floorf((cx + half) / GSTEP);
  int iz1 = (int)floorf((cz + half) / GSTEP);
  gnx = ix1 - gix0 + 1;
  cellNext = 0;
  cellEnd = gnx * (iz1 - giz0 + 1);
}

// Eine Zelle (wie der alte grass_build, aber int-Index statt FP-Akkumulation)
static void build_cell(int ix, int iz){
  float x = ix * GSTEP, z = iz * GSTEP;
  float h1 = c_hash12(ix * 0.123f, iz * 0.457f);
  float h2 = c_hash12(ix * 0.457f, iz * 0.123f);
  float h3 = c_hash12(ix * 0.711f, iz * 0.271f);
  float h4 = c_hash12(ix * 0.271f, iz * 0.711f);
  float px = x + (h1 - 0.5f) * 0.5f * GSTEP;
  float pz = z + (h2 - 0.5f) * 0.5f * GSTEP;
  // Distanz zur Kamera -> Dichte-Falloff
  float dx = px - bcx, dz = pz - bcz;
  float dist = sqrtf(dx * dx + dz * dz);
  if(dist > g_half()) return;
  // Dichte: nah 1.0, fern 0.0. Weicher Falloff (kein plötzliches Null):
  //   - Falloff von 8m bis grassMaxDist (skaliert mit Sichtweite-Slider)
  //   - Pow 1.6: nimmt in der Ferne starker ab (nah bleibt dicht)
  //   - Kein Hard-Cutoff bei 0.05, sondern weicher Abbruch bei 0.01
  float dens = powf(1.0f - c_smoothstep(8.0f, grassMaxDist, dist), 1.6f);
  if(dens < 0.01f) return;
  float h = c_terrainH(px, pz);
  if(h < WATER_Y + 0.10f || h > 7.5f) return;  // kein Wasser, keine Gipfel
  if(fabsf(c_terrainH(px + 0.4f, pz) - h) > 0.25f ||
     fabsf(c_terrainH(px, pz + 0.4f) - h) > 0.25f) return;  // Steilhang
  float cl = c_fbm(px * 0.12f, pz * 0.12f);       // Gras-Flecken
  if(h3 > cl + 0.50f) return;                     // Flecken
  // Anzahl Halme: 4x mehr im Nahbereich (war 1+5*dens, jetzt 4+20*dens),
  // skaliert mit Dichte und Dichte-Skala (Slider)
  int nb = (int)(4.0f + 20.0f * dens * (0.4f + 0.6f * h4) * grassDensityScale);
  if(nb < 1) nb = 1;
  if(nb > 12) nb = 12;
  // Halmgroesse: BASIS-Werte ohne Distanz-Skalierung. Die Distanz-Skalierung
  // (groesser in der Ferne, um Flaechen abzudecken) passiert im Shader
  // (grass.vert) kontinuierlich zur aktuellen Kameradistanz.
  for(int b = 0; b < nb; b++){
    if(nGrassNew >= GRASS_CAP) return;
    float b1 = c_hash12(ix * 1.13f + b, iz * 1.71f + b);
    float b2 = c_hash12(ix * 1.71f + b, iz * 1.13f + b);
    float b3 = c_hash12(ix * 2.13f + b, iz * 2.71f + b);
    float b4 = c_hash12(ix * 2.71f + b, iz * 2.13f + b);
    float b5 = c_hash12(ix * 3.13f + b, iz * 3.71f + b);
    float b6 = c_hash12(ix * 3.71f + b, iz * 3.13f + b);
    float bx = px + (b1 - 0.5f) * 0.28f;
    float bz = pz + (b2 - 0.5f) * 0.28f;
    float bh = c_terrainH(bx, bz);
    if(bh < WATER_Y + 0.10f) continue;
    float *d = &buildBuf[nGrassNew * GRASS_FLOATS];
    d[0] = bx; d[1] = bh - 0.03f; d[2] = bz;      // Basis leicht eingegraben
    d[3] = 0.22f + 0.30f * b3 + 0.12f * cl;    // Halmhoehe (Basis)
    d[4] = 0.026f + 0.022f * b4;               // Halmbreite (Basis)
    d[5] = b1 * 6.283f;                           // Wind-Phase
    d[6] = 0.75f + 0.5f * b2;                     // Schattierung
    d[7] = b5 * 6.283f;                           // Yaw (Ebenen-Rotation)
    d[8] = 0.15f + 0.45f * b6;                    // Lean (Spitzen-Neigung)
    // Birth-Time: Placeholder (0.0), wird in grass_build_finish gesetzt
    //  (erst beim Swap, nicht waehrend des Builds, sonst sind Halme schon "alt")
    d[9] = 0.0f;
    nGrassNew++;
  }
}

int grass_build_step(int maxCells){
  for(int i = 0; i < maxCells && cellNext < cellEnd; i++, cellNext++){
    int ix = gix0 + cellNext % gnx;
    int iz = giz0 + cellNext / gnx;
    build_cell(ix, iz);
    if(nGrassNew >= GRASS_CAP){ cellNext = cellEnd; break; }
  }
  return cellNext >= cellEnd;
}

void grass_build_finish(void){
  // Birth-Time setzen (erst beim Swap, nicht waehrend des Builds):
  //  - Halme, die im alten Build existierten: "alt" (sofort sichtbar, birth = -10s)
  //  - Halme außerhalb: "neu" (blenden ein, birth = jetzt)
  // "Existiert im alten Build" = im alten Dichte-Bereich (gleiche Bedingung wie
  //  build_cell: dens >= 0.01). NICHT der alte Build-Radius (grassMaxDist+20):
  //  dort gibt es gar keine Halme (dens < 0.01), d.h. der Radius-Check hat bei
  //  normalen Kamerabewegungen (< ~30m) KEINEN einzigen Halm als "neu" erkannt
  //  -> Fade war totes Brett, alles erschien instantan.
  int nNew = 0;
  for (int i = 0; i < nGrassNew; i++) {
    float *d = &buildBuf[i * GRASS_FLOATS];
    float bx = d[0], bz = d[2];
    if (hasOldBuild) {
      float odx = bx - old_bcx, odz = bz - old_bcz;
      float odist = sqrtf(odx * odx + odz * odz);
      float odens = powf(1.0f - c_smoothstep(8.0f, grassMaxDist, odist), 1.6f);
      d[9] = (odens >= 0.01f) ? (gWorldTime - 10.0f) : gWorldTime;
      if (d[9] >= gWorldTime) nNew++;
    } else {
      d[9] = gWorldTime - 10.0f;   // erster Build: sofort sichtbar
    }
  }
  if (nNew > 0)
    printf("Gras-Swap: %d Halme, %d neu (blenden 2s ein)\n", nGrassNew, nNew);
  if (nGrassNew < 1) nGrassNew = 1;   // defensive: kein 0-Byte-Upload
  pthread_mutex_lock(&gMu);
  hasOldBuild = 1;
  float *tmp = grassInst; grassInst = buildBuf; buildBuf = tmp;
  nGrass = nGrassNew;
  gBuildDone = 1;   // Main-Thread holt den Build per grass_build_finished_snapshot()
  pthread_mutex_unlock(&gMu);
}

// ================= Build-Thread =================
// Der Generator laeuft im eigenen Thread, damit er bei schneller
// Kamerabewegung nicht hinterherhinkt (war: 8000 Zellen/Frame im Main-Loop).
// Der Main-Thread ruft grass_build_request() (neues Zentrum) und holt fertige
// Builds per grass_build_finished_snapshot() ab. Der VBO-Upload bleibt im
// Main-Thread (GL-Kontext laeuft dort).
//
// "Latest wins": Ein neuer Request waehrend eines Builds restartet den Build
// mit dem jeweils aktuellen Zentrum (in Chunks gecheckt).
void grass_build_request(float cx, float cz){
  pthread_mutex_lock(&gMu);
  gReqCX = cx; gReqCZ = cz;
  gReqSeq++;
  gReqPending = 1;
  pthread_cond_signal(&gCv);
  pthread_mutex_unlock(&gMu);
}

// Fertigen Build abholen (exakt einmalig): buf = Buffer, n = Halm-Count.
// buf == NULL, wenn nichts fertig ist. Rufer macht danach den VBO-Upload.
void grass_build_finished_snapshot(float **buf, int *n){
  pthread_mutex_lock(&gMu);
  if (gBuildDone) { *buf = grassInst; *n = nGrass; gBuildDone = 0; }
  else *buf = NULL;
  pthread_mutex_unlock(&gMu);
}

static void* grass_worker(void* arg){
  (void)arg;
  for(;;){
    pthread_mutex_lock(&gMu);
    while(!gReqPending) pthread_cond_wait(&gCv, &gMu);
    gReqPending = 0;
    float cx = gReqCX, cz = gReqCZ;
    int seq = gReqSeq;
    pthread_mutex_unlock(&gMu);
    grass_build_begin(cx, cz);
    // In Chunks bauen; neuer Request (hoeheres gReqSeq) = Restart mit
    // dem jeweils aktuellen Zentrum.
    for(;;){
      if(grass_build_step(16000)) break;
      pthread_mutex_lock(&gMu);
      if(gReqSeq != seq){
        seq = gReqSeq;
        grass_build_begin(gReqCX, gReqCZ);
      }
      pthread_mutex_unlock(&gMu);
    }
    grass_build_finish();
  }
  return NULL;
}

int grass_thread_start(void){
  pthread_t t;
  if(pthread_create(&t, NULL, grass_worker, NULL) != 0) return 0;
  pthread_detach(t);
  return 1;
}
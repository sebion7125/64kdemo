// grass_cpu.c - prozedurales Gras (instanzierte Halme, reines C, kein GL).
//
// Dichte nimmt mit der Distanz zur Kamera ab (nah: dicht, fern: lichter).
// Halme werden mit der Distanz groesser (decken die Flaechen ab) ->
// einheitliche visuelle Dichte. Die Groessen-Skalierung passiert im Shader
// (grass.vert) kontinuierlich zur aktuellen Kameradistanz, die CPU speichert
// nur Basis-Hoehe/-Breite -> springt nicht beim Rebuild.
// Zell-Hashes aus Weltkoordinaten -> Gras bleibt stabil bei Patch-Rebuilds.
//
// Inkrementeller Rebuild (Generator laeuft im Build-Thread):
//   begin  startet einen neuen Build in den zweiten Buffer (alt bleibt sichtbar)
//   step   verarbeitet max. N Zellen (liefert 1 wenn fertig)
//   finish tauscht die Buffer; main.c uploadet danach den VBO
//
// Zell-Kopieren (schnelle Rebuilds bei kleinen Kameraverschiebungen):
//   Halm-Daten haengen nur von (ix,iz,b) ab (Hashes), NICHT von der Distanz
//   zur Kamera (Groessen-Skalierung ist im Shader). Zellen, deren Halm-Anzahl
//   nb beim Rebuild unveraendert bleibt, werden daher kopiert statt neu
//   generiert -> 1m-Verschiebung in ~100ms statt ~700ms.
//
// Halm-Daten (GRASS_FLOATS=10): pos(3) hoehe breite phase schattierung yaw lean birth
//   yaw  = Rotation der Halm-Ebene um Y (sichtbar aus allen Richtungen)
//   lean = Neigung der Spitze in yaw-Richtung (sichtbar von oben)
#include "scene.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

static float gA[GRASS_CAP * GRASS_FLOATS];
static float gB[GRASS_CAP * GRASS_FLOATS];
float *grassInst = gA;   // aktuell sichtbar (VBO-Inhalt)
int nGrass = 0;

static float *buildBuf = gB;
static int nGrassNew = 0;
static int cix = 0, ciz = 0, gnz = 0;   // aktuelle Zelle (inkrementell, kein div/mod)
static int gix0 = 0, giz0 = 0, gnx = 0;
static float bcx = 0, bcz = 0;
// Alter Build-Zentrum (fuer Fade-in: nur Halme außerhalb des alten Bereichs sind "neu")
static float old_bcx = 0, old_bcz = 0;
static int hasOldBuild = 0;

// Build-Thread-Synchronisation (siehe grass_worker am Dateiende):
//   gReqPending = Request wartet, gBuildDone = Build fertig (Abholung)
static pthread_mutex_t gMu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  gCv = PTHREAD_COND_INITIALIZER;
static int   gReqPending = 0, gBuildDone = 0;
static float gReqCX = 0, gReqCZ = 0;

// Zell-Index-Tabellen (eine pro Halm-Buffer): Start-Index und Halm-Anzahl
// pro Weltzelle (ix,iz absolut -> 1:1-Mapping ueber Builds hinweg).
// CT×CT deckt das groesste Build-Quadrat ab (MD=120 -> ~693 Zellen/Sseite).
#define CT 700
static int      cellOffA[CT * CT], cellOffB[CT * CT];
static uint8_t  cellCntA[CT * CT], cellCntB[CT * CT];
static int     *cellOff = cellOffA;     // Tabelle des alten (sichtbaren) Builds
static uint8_t *cellCnt = cellCntA;
static int     *buildOff = cellOffB;    // Tabelle des aktuellen Builds
static uint8_t *buildCnt = cellCntB;
static int oldOffX0 = 0, oldOffZ0 = 0;  // Tabellen-Origin des alten Builds
static int offX0 = 0, offZ0 = 0;        // Tabellen-Origin des aktuellen Builds

#define GSTEP 0.30f
// Build-Radius = Dichte-Cutoff (wo dens auf 0.01 faellt), NICHT
// grassMaxDist+20: auBerhalb generiert build_cell keine Halme (dens < 0.01),
// der alte +20m-Margin hat nur Build-Zeit gekostet. Cutoff = Loesung von
// dens(d) = 0.01 (Bisektion, pro grassMaxDist gecacht).
static float gDCut = 0.0f, gDCutMD = -1.0f;
static float g_half(void){
  float md = grassMaxDist;
  if (md != gDCutMD) {
    float lo = 8.0f, hi = md;
    for (int i = 0; i < 30; i++) {
      float mid = (lo + hi) * 0.5f;
      float dens = powf(1.0f - c_smoothstep(8.0f, md, mid), 1.6f);
      if (dens >= 0.01f) lo = mid; else hi = mid;
    }
    gDCut = (lo + hi) * 0.5f;
    gDCutMD = md;
  }
  return gDCut;
}

// Dichte-LUT: dens = f(dist) ist radial -> Tabelle statt powf (macht den
// Zell-Kopier-Pfad deutlich schneller). 0.5m-Resolution, pro grassMaxDist
// neu befuellt (Slider).
#define DENS_LUT_N 480   // 0..240m
static float densLut[DENS_LUT_N];
static float densLutMD = -1.0f;
static float dens_at(float dist){
  float md = grassMaxDist;
  if (md != densLutMD) {
    for (int i = 0; i < DENS_LUT_N; i++)
      densLut[i] = powf(1.0f - c_smoothstep(8.0f, md, i * 0.5f), 1.6f);
    densLutMD = md;
  }
  int i = (int)(dist * 2.0f);
  if (i >= DENS_LUT_N) i = DENS_LUT_N - 1;
  return densLut[i];
}

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
  gnz = iz1 - giz0 + 1;
  if (gnx > CT) gnx = CT;   // defensive: Tabelle ist CT×CT
  if (gnz > CT) gnz = CT;
  offX0 = gix0; offZ0 = giz0;
  cix = 0; ciz = 0;
}

// Eine Zelle: kopieren oder generieren.
//   Kopieren: Zelle existiert im alten Build mit gleichem nb (rechnerisch,
//     zell-zentriert) -> Halm-Daten identisch (Hashes nur aus (ix,iz,b),
//     Distanz/Dichte spielen nicht rein) -> alte Halme kopieren (Birth-Time
//     bleibt erhalten: laufender Fade geht nahtlos weiter).
//   Generieren: neue Zelle oder nb geaendert (Slider/Distanz) -> voll.
static void build_cell(int ix, int iz){
  float x = ix * GSTEP, z = iz * GSTEP;
  int ixl = ix - gix0, izl = iz - giz0;
  buildOff[izl * CT + ixl] = -1;
  buildCnt[izl * CT + ixl] = 0;
  // Schnelle Ablehnung: auBerhalb des Build-Radius (quadratisch, kein sqrt)
  float dx = x - bcx, dz = z - bcz;
  float half = g_half();
  if (dx * dx + dz * dz > half * half) return;
  float h4 = c_hash12(ix * 0.271f, iz * 0.711f);
  // --- Kopier-Entscheidung (vor der teuren Terrain-Berechnung) ---
  int oldIdx = -1;
  if (hasOldBuild) {
    int ox = ix - oldOffX0, oz = iz - oldOffZ0;
    if (ox >= 0 && ox < CT && oz >= 0 && oz < CT) {
      int oi = oz * CT + ox;
      if (cellOff[oi] >= 0) oldIdx = oi;
    }
  }
  if (oldIdx >= 0) {
    float dist = sqrtf(dx * dx + dz * dz);
    float dens = dens_at(dist);
    if (dens >= 0.01f) {
      int nb = (int)(4.0f + 20.0f * dens * (0.4f + 0.6f * h4) * grassDensityScale);
      if (nb < 1) nb = 1;
      if (nb > 12) nb = 12;
      // altes nb mit den aktuellen Parametern (altes Zentrum): gleich =
      // Halm-Menge unveraendert -> kopieren
      float odx = x - old_bcx, odz = z - old_bcz;
      float odist = sqrtf(odx * odx + odz * odz);
      float odens = dens_at(odist);
      int onb = (int)(4.0f + 20.0f * odens * (0.4f + 0.6f * h4) * grassDensityScale);
      if (onb < 1) onb = 1;
      if (onb > 12) onb = 12;
      if (onb == nb) {
        int cnt = cellCnt[oldIdx];
        if (nGrassNew + cnt > GRASS_CAP) cnt = GRASS_CAP - nGrassNew;
        const float *src = &grassInst[cellOff[oldIdx] * GRASS_FLOATS];
        float *dst = &buildBuf[nGrassNew * GRASS_FLOATS];
        for (int i = 0; i < cnt * GRASS_FLOATS; i++) dst[i] = src[i];
        nGrassNew += cnt;
        buildOff[izl * CT + ixl] = nGrassNew - cnt;
        buildCnt[izl * CT + ixl] = (uint8_t)cnt;
        return;
      }
    }
  }
  // --- Vollstaendige Generierung ---
  float h1 = c_hash12(ix * 0.123f, iz * 0.457f);
  float h2 = c_hash12(ix * 0.457f, iz * 0.123f);
  float h3 = c_hash12(ix * 0.711f, iz * 0.271f);
  float px = x + (h1 - 0.5f) * 0.5f * GSTEP;
  float pz = z + (h2 - 0.5f) * 0.5f * GSTEP;
  // Distanz zur Kamera -> Dichte-Falloff
  float pxd = px - bcx, pzd = pz - bcz;
  float dist = sqrtf(pxd * pxd + pzd * pzd);
  if(dist > half) return;
  // Dichte: nah 1.0, fern 0.0. Weicher Falloff (kein plötzliches Null):
  //   - Falloff von 8m bis grassMaxDist (skaliert mit Sichtweite-Slider)
  //   - Pow 1.6: nimmt in der Ferne starker ab (nah bleibt dicht)
  //   - Kein Hard-Cutoff bei 0.05, sondern weicher Abbruch bei 0.01
  float dens = dens_at(dist);
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
  // Clumping: Voronoi-artige Buenschel auf grobem 8m-Raster. Jede Rasterzelle
  // hat einen Cluster-Mittelpunkt (Hash) und eine Dichte-Varianz (Hash);
  // Halme nah am Mittelpunkt sind wahrscheinlicher -> sichtbare Buenschel
  // statt gleichmaessiger Streuung. Haengt nur von (x,z) ab (nicht von der
  // Kamera) -> die Copy-Entscheidung (basis-nb-Vergleich) bleibt gueltig.
  {
    int cx8 = (int)floorf(x / 8.0f), cz8 = (int)floorf(z / 8.0f);
    float hcx = c_hash12(cx8 * 0.731f, cz8 * 0.317f);
    float hcz = c_hash12(cx8 * 0.271f, cz8 * 0.913f);
    float hcd = c_hash12(cx8 * 0.557f, cz8 * 0.137f);
    float dcx = x - (cx8 + 0.15f + 0.7f * hcx) * 8.0f;
    float dcz = z - (cz8 + 0.15f + 0.7f * hcz) * 8.0f;
    float dc = sqrtf(dcx * dcx + dcz * dcz);
    float clump = (0.35f + 0.65f * (1.0f - c_smoothstep(0.0f, 5.5f, dc)))
                 * (0.6f + 0.4f * hcd);
    nb = (int)(nb * clump);
    if(nb > 12) nb = 12;
  }
  // Halmgroesse: BASIS-Werte ohne Distanz-Skalierung. Die Distanz-Skalierung
  // (groesser in der Ferne, um Flaechen abzudecken) passiert im Shader
  // (grass.vert) kontinuierlich zur aktuellen Kameradistanz.
  int start = nGrassNew;
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
  int cnt = nGrassNew - start;
  if (cnt > 0) {
    buildOff[izl * CT + ixl] = start;
    buildCnt[izl * CT + ixl] = (uint8_t)cnt;
  }
}


int grass_build_step(int maxCells){
  // (ix,iz) inkrementell fortschreiben: cellNext % gnx / gnx waere pro Zelle
  // eine Integer-Division (idivl ~30 Zyklen).
  for(int i = 0; i < maxCells && ciz < gnz; i++){
    build_cell(gix0 + cix, giz0 + ciz);
    if(nGrassNew >= GRASS_CAP){ ciz = gnz; break; }
    if(++cix >= gnx){ cix = 0; ciz++; }
  }
  return ciz >= gnz;
}

void grass_build_finish(void){
  // Birth-Time setzen (erst beim Swap, nicht waehrend des Builds):
  //  - kopierte Halme (d[9] != 0): alte Birth-Time behalten (Fade laeuft weiter)
  //  - generierte Halme (d[9] == 0): im alten Dichte-Bereich "alt" (sofort
  //    sichtbar, birth = -10s), außerhalb "neu" (blenden ein, birth = jetzt).
  // "Im alten Dichte-Bereich" = odist <= Dichte-Cutoff (quadratischer
  // Vergleich, kein sqrt/pow). Der alte Build-Radius taugt nicht: dort gibt
  // es gar keine Halme (dens < 0.01) -> der Radius-Check hatte bei normalen
  // Kamerabewegungen keinen einzigen Halm als "neu" erkannt (Fade tot).
  float dCut = g_half();
  float dCut2 = dCut * dCut;
  int nNew = 0;
  for (int i = 0; i < nGrassNew; i++) {
    float *d = &buildBuf[i * GRASS_FLOATS];
    if (d[9] != 0.0f) continue;   // kopierter Halm: Birth-Time erhalten
    float bx = d[0], bz = d[2];
    if (hasOldBuild) {
      float odx = bx - old_bcx, odz = bz - old_bcz;
      d[9] = (odx * odx + odz * odz <= dCut2) ? (gWorldTime - 10.0f) : gWorldTime;
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
  int *to = cellOff; cellOff = buildOff; buildOff = to;
  uint8_t *tc = cellCnt; cellCnt = buildCnt; buildCnt = tc;
  oldOffX0 = offX0; oldOffZ0 = offZ0;
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
// "Finish then latest": Ein Build wird immer zu Ende gebaut (kein Restart
// mitten drin); Requests während des Builds warten, und der naechste Build
// geht direkt zum jeweils neuesten Zentrum. Mit Zell-Kopieren dauert ein
// Build bei kleinen Verschiebungen nur ~100ms -> das Gras hinkt der
// Kamera hochstens ~1-2m hinterher.
void grass_build_request(float cx, float cz){
  pthread_mutex_lock(&gMu);
  gReqCX = cx; gReqCZ = cz;
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
    pthread_mutex_unlock(&gMu);
    grass_build_begin(cx, cz);
    while(!grass_build_step(1 << 30)) {}
    grass_build_finish();
    // Request, der während des Builds kam: gReqPending ist gesetzt ->
    // die Schleife startet sofort den naechsten Build auf das neueste Zentrum.
  }
  return NULL;
}

int grass_thread_start(void){
  pthread_t t;
  if(pthread_create(&t, NULL, grass_worker, NULL) != 0) return 0;
  pthread_detach(t);
  return 1;
}
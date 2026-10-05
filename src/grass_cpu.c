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
// Halm-Daten (GRASS_FLOATS=11): pos(3) hoehe breite phase schattierung yaw lean birth death
//   yaw  = Rotation der Halm-Ebene um Y (sichtbar aus allen Richtungen)
//   lean = Neigung der Spitze in yaw-Richtung (sichtbar von oben)
//   birth = Birth-Time (2s Fade-in), death = Death-Time (2s Fade-out, 0 = lebt)
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
static int gnz = 0;
static int dr = 0, drp = 0, dcx0 = 0, dcz0 = 0, dmaxR = 0;   // Diamond-Order-State
static int gix0 = 0, giz0 = 0, gnx = 0;
static float bcx = 0, bcz = 0;
static int outStepState = 0;   // OUT-Build: naechster Tabellen-Index
// Alter Build-Zentrum (fuer Fade-in: nur Halme außerhalb des alten Bereichs sind "neu")
static float old_bcx = 0, old_bcz = 0;
static int hasOldBuild = 0;
// Build-Modi:
//  0 = POS: Positions-Rebuild (Zell-Kopieren; neue Halme faden ein,
//           ueberzaehlige - nb gesunken - faden aus)
//  1 = OUT: Slider-Phase 1 — alle alten Halme sterben (d[10]=jetzt),
//           keine Generierung (altes Gras blendet 2s aus)
//  2 = IN:  Slider-Phase 2 / erster Build — alles neu generieren
//           (d[9]=jetzt, neues Gras blendet 2s ein)
static int gBuildMode = 0;

// Build-Thread-Synchronisation (siehe grass_worker am Dateiende):
//   gReqPending = Request wartet, gBuildDone = Build fertig (Abholung)
static pthread_mutex_t gMu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  gCv = PTHREAD_COND_INITIALIZER;
static int   gReqPending = 0, gBuildDone = 0;
static int   gBuilding = 0;   // Worker baut gerade (fuer HUD-Indikator)
static float gReqCX = 0, gReqCZ = 0;
static int   gReqMode = 0;   // Build-Modus (0=POS 1=OUT 2=IN)

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
static int old_gnx = 0, old_gnz = 0;    // Tabellen-Groesse des alten Builds
static int offX0 = 0, offZ0 = 0;        // Tabellen-Origin des aktuellen Builds

#define GSTEP 0.30f
#define CLUMP 1.2f   // Bueschel-Grid (Schritt 4): naechster Mittelpunkt in 3x3-Zellen
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
      float dens = powf(1.0f - c_smoothstep(8.0f, md, mid), 1.0f);
      // Schwelle 0.09: Kante wo die mittlere Halmzahl ~2 ist (32*dens*0.7).
      // (Frueher 0.01: die 1m-Crescent-Zone beim Positions-Shift hatte
      // dens < 0.01 -> 0 Halme/Zelle -> nichts zum Einblenden (Fade tot).)
      if (dens >= 0.09f) lo = mid; else hi = mid;
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
      densLut[i] = powf(1.0f - c_smoothstep(8.0f, md, i * 0.5f), 1.0f);
    densLutMD = md;
  }
  int i = (int)(dist * 2.0f);
  if (i >= DENS_LUT_N) i = DENS_LUT_N - 1;
  return densLut[i];
}

void grass_build_begin(float cx, float cz, int mode){
  // Altes Zentrum merken (fuer POS-Builds: altes nb mit altem Zentrum).
  // hasOldBuild wird erst in grass_build_finish gesetzt (erster Build =
  // alle Halme sofort sichtbar, kein Vergleich gegen das (0,0)-Zentrum).
  if (hasOldBuild) { old_bcx = bcx; old_bcz = bcz; }
  bcx = cx; bcz = cz;
  gBuildMode = mode;
  nGrassNew = 0;
  buildBuf = (grassInst == gA) ? gB : gA;
  // Build-Tabelle leeren: auBerhalb des Build-Quadrats darf nichts altes
  // stehen (das Zell-Lookup liest cellOff[oi] >= 0 als "Zelle existiert").
  for (int i = 0; i < CT * CT; i++) { buildOff[i] = -1; buildCnt[i] = 0; }
  if (mode == 1) {
    // OUT-Build: geht die ALTE Zell-Tabelle direkt durch (kein Radius-Walk).
    // Tabelle/Origin wie beim alten Build (damit naechste Builds die
    // sterbenden Halme in ihren Zellen finden).
    offX0 = oldOffX0; offZ0 = oldOffZ0;
    outStepState = 0;
    return;
  }
  float half = g_half();
  gix0 = (int)floorf((cx - half) / GSTEP);
  giz0 = (int)floorf((cz - half) / GSTEP);
  int ix1 = (int)floorf((cx + half) / GSTEP);
  int iz1 = (int)floorf((cz + half) / GSTEP);
  gnx = ix1 - gix0 + 1;
  gnz = iz1 - giz0 + 1;
  if (gnx > CT) gnx = CT;   // defensive: Tabelle ist CT×CT
  if (gnz > CT) gnz = CT;
  if (mode == 0 && hasOldBuild) {
    // Walk-Quadrat = UNION von neuem Radius-Quadrat und altem Walk-Quadrat.
    // Die Kanten-Sichel (Zellen auBerhalb des neuen Radius, aber im alten
    // Quadrat: sterbende Halme aus den letzten ~2.5s) wird mitverarbeitet
    // und fadet 2s aus statt sofort zu pochen (Rueckwaertsfahrt: Gras-Kante
    // verschwand ploetzlich pro 1m-Shift). Selbstbegrenzt: Halme aelter als
    // 2.5s werden beim Kopieren verworfen, das Quadrat waechst max. mit der
    // Kamerageschwindigkeit * 2.5s.
    int ux0 = gix0, uy0 = giz0, ux1 = gix0 + gnx - 1, uy1 = giz0 + gnz - 1;
    if (oldOffX0 < ux0) ux0 = oldOffX0;
    if (oldOffZ0 < uy0) uy0 = oldOffZ0;
    if (oldOffX0 + old_gnx - 1 > ux1) ux1 = oldOffX0 + old_gnx - 1;
    if (oldOffZ0 + old_gnz - 1 > uy1) uy1 = oldOffZ0 + old_gnz - 1;
    gix0 = ux0; giz0 = uy0;
    gnx = ux1 - ux0 + 1; gnz = uy1 - uy0 + 1;
    if (gnx > CT) gnx = CT;
    if (gnz > CT) gnz = CT;
  }
  offX0 = gix0; offZ0 = giz0;
  // Diamond-Order: Zellen vom Zentrum (Kamera) nach auessen verarbeited.
  // Wenn GRASS_CAP erreicht wird, fehlen die WEAESTEN Zellen (frueher:
  // Gitter-Order von der Ecke -> naehere Zellen konnten fehlen).
  dcx0 = (int)floorf(cx / GSTEP) - gix0;
  dcz0 = (int)floorf(cz / GSTEP) - giz0;
  if (dcx0 < 0) dcx0 = 0;
  if (dcx0 >= gnx) dcx0 = gnx - 1;
  if (dcz0 < 0) dcz0 = 0;
  if (dcz0 >= gnz) dcz0 = gnz - 1;
  dmaxR = (dcx0 > gnx - 1 - dcx0 ? dcx0 : gnx - 1 - dcx0)
        + (dcz0 > gnz - 1 - dcz0 ? dcz0 : gnz - 1 - dcz0);
  dr = 0; drp = 0;
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
  // AuBerhalb des NEUEN Build-Radius: nur Zellen mit altem Gras verarbeiten
  // (Kanten-Sichel: ihre Halme faden 2s aus statt sofort zu pochen; der
  // Walk-Radius deckt die Union der alten/neuen Disks ab, s. begin).
  float dx = x - bcx, dz = z - bcz;
  float half = g_half();
  int outsideNew = dx * dx + dz * dz > half * half;
  float h4 = c_hash12(ix * 0.271f, iz * 0.711f);
  // --- Kopier-Entscheidung (vor der teuren Terrain-Berechnung) ---
  // Halm-Daten sind eine reine Funktion von (ix,iz,b) (Hashes + Terrain,
  // NICHT von der Kameradistanz) -> Halme koennen kopiert statt regeneriert
  // werden. Pro Halm (d[10] = Death-Time, 0 = lebt):
  //   - lebt und nb reicht noch  -> kopieren (Birth-Time bleibt, Fade weiter)
  //   - lebt, aber nb gesunken   -> kopieren + sterben (d[10]=jetzt, 2s aus)
  //   - stirbt schon             -> kopieren, bis der 2s-Fade zu Ende ist
  //   - stirbt, Fade zu Ende     -> weg
  // Neue Halme (nb > altes nb) generieren ab b=onb und blenden 2s ein.
  // Nur bei POS-Builds (Modus 0); IN (Modus 2) generiert alles neu.
  int oldIdx = -1;
  int bStart = 0;   // erster zu generierender b-Index (0 = volle Generierung)
  int copied = 0;   // kopierte Halme dieser Zelle (fuer buildOff/buildCnt)
  int partial = 0;  // Teil-Kopie: Zelle bestockt, nur Teil neu generiert
  if (gBuildMode == 0 && hasOldBuild) {
    int ox = ix - oldOffX0, oz = iz - oldOffZ0;
    if (ox >= 0 && ox < CT && oz >= 0 && oz < CT) {
      int oi = oz * CT + ox;
      if (cellOff[oi] >= 0) oldIdx = oi;
    }
  }
  if (outsideNew && oldIdx < 0) return;   // neue Zelle auBerhalb: nichts zu tun
  if (oldIdx >= 0) {
    float dist = sqrtf(dx * dx + dz * dz);
    float dens = dens_at(dist);
    int nb = 0;
    if (dens >= 0.01f) {
      float nbf = 32.0f * dens * (0.4f + 0.6f * h4) * grassDensityScale;
      if (nbf > 32.0f) nbf = 32.0f;
      nb = (int)nbf;
      if (c_hash12(ix * 0.917f, iz * 0.311f) < nbf - (float)nb) nb++;   // stochastisch
    }
    // altes nb mit den aktuellen Parametern (altes Zentrum): ab welchem
    // b-Index es keine alten Halme mehr gibt (neue Halme kommen ab da)
    float odx = x - old_bcx, odz = z - old_bcz;
    float odist = sqrtf(odx * odx + odz * odz);
    float odens = dens_at(odist);
    float onbf = 32.0f * odens * (0.4f + 0.6f * h4) * grassDensityScale;
    if (onbf > 32.0f) onbf = 32.0f;
    int onb = (int)onbf;
    if (c_hash12(ix * 0.917f, iz * 0.311f) < onbf - (float)onb) onb++;   // stochastisch
    // Alte Halme durchgehen: leben lassen, sterben lassen oder weg
    int ocnt = cellCnt[oldIdx];
    int ostart = cellOff[oldIdx];
    int alive = 0;
    for (int p = 0; p < ocnt; p++) {
      const float *src = &grassInst[(ostart + p) * GRASS_FLOATS];
      float death = src[10];
      int copyIt, nowDying;
      if (death > 0.0f) {
        copyIt = (gWorldTime - death < 2.5f);   // Fade-out noch nicht zu Ende
        nowDying = 0;                            // Death-Time behalten
      } else if (alive < nb) {
        copyIt = 1; nowDying = 0; alive++;
      } else {
        copyIt = 1; nowDying = 1;                // ueberzaehlig: sterben
      }
      if (!copyIt) continue;
      if (nGrassNew >= GRASS_CAP) break;
      float *dst = &buildBuf[nGrassNew * GRASS_FLOATS];
      for (int k = 0; k < GRASS_FLOATS; k++) dst[k] = src[k];
      if (nowDying) dst[10] = gWorldTime;
      nGrassNew++;
      copied++;
    }
    if (nb > onb && nGrassNew + (nb - onb) <= GRASS_CAP) {
      bStart = onb;   // nb > onb: nur die letzten Halme sind neu
      partial = 1;
    } else {
      // nb <= onb (nur kopieren/sterben) oder CAP: kein Platz fuer neue
      if (copied > 0) {
        buildOff[izl * CT + ixl] = nGrassNew - copied;
        buildCnt[izl * CT + ixl] = (uint8_t)copied;
      }
      return;
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
  if(dist > half && !partial) return;   // Teil-Kopie: Zelle ist bestockt
  // Dichte: nah 1.0, fern 0.0. Weicher Falloff (kein plötzliches Null):
  //   - Falloff von 8m bis grassMaxDist (skaliert mit Sichtweite-Slider)
  //   - Pow 1.6: nimmt in der Ferne starker ab (nah bleibt dicht)
  //   - Kein Hard-Cutoff bei 0.05, sondern weicher Abbruch bei 0.01
  float dens = dens_at(dist);
  if(dens < 0.09f && !partial) return;   // Teil-Kopie: Zelle ist bestockt
  float h = c_terrainH(px, pz);
  if(h < WATER_Y + 0.10f || h > 7.5f) return;  // kein Wasser, keine Gipfel
  if(fabsf(c_terrainH(px + 0.4f, pz) - h) > 0.25f ||
     fabsf(c_terrainH(px, pz + 0.4f) - h) > 0.25f) return;  // Steilhang
  float cl = c_fbm(px * 0.12f, pz * 0.12f);       // Gras-Flecken
  if(h3 > cl + 0.50f) return;                     // Flecken
  // Anzahl Halme: 32*dens (ohne Basis-Offset) -> laeuft weich gegen 0 an der
  // Sichtweite-Kante (kein harter Gras-Rand mehr). Cap 32 laesst dem
  // Dichte-Slider Spielraum im Nahbereich. Stochastisches Runden: der
  // Bruchteil kommt mit seiner Wahrscheinlichkeit dazu (Kante bleibt
  // bestockt -> der 1m-Shift-Crescent hat Halme -> Fade-in funktioniert).
  float nbf = 32.0f * dens * (0.4f + 0.6f * h4) * grassDensityScale;
  if (nbf > 32.0f) nbf = 32.0f;
  int nb = (int)nbf;
  if (c_hash12(ix * 0.917f, iz * 0.311f) < nbf - (float)nb) nb++;
  // Halmgroesse: BASIS-Werte ohne Distanz-Skalierung. Die Distanz-Skalierung
  // (groesser in der Ferne, um Flaechen abzudecken) passiert im Shader
  // (grass.vert) kontinuierlich zur aktuellen Kameradistanz.
  // Clumping (Schritt 4): naechster Bueschel-Mittelpunkt (1.2m-Grid, 3x3).
  // Einmal pro ZELLE gesucht (am Zellzentrum, nicht pro Halm: 18 Sin-Hashes
  // pro Halm waeren zu teuer fuer 1m-Rebuilds; Halm-Offsets in der Zelle
  // sind ±0.14m = 12% von CLUMP, visuell vernachlaessigbar). Der Einfluss
  // w wird pro Halm aus der eigenen Distanz zum Mittelpunkt berechnet.
  int cix = (int)floorf(px / CLUMP), ciz = (int)floorf(pz / CLUMP);
  float cxm = px, czm = pz, cid = 0.0f;
  if (nb > 0) {
    float best = 1e9f;
    for (int j = -1; j <= 1; j++)
      for (int i = -1; i <= 1; i++) {
        float hx = c_hash12((cix + i) * 0.731f, (ciz + j) * 0.219f);
        float hz = c_hash12((cix + i) * 0.219f, (ciz + j) * 0.731f);
        float mx = (cix + i + hx) * CLUMP, mz = (ciz + j + hz) * CLUMP;
        float d2 = (px - mx) * (px - mx) + (pz - mz) * (pz - mz);
        if (d2 < best) { best = d2; cxm = mx; czm = mz; cid = c_hash12(mx, mz); }
      }
  }
  int start = nGrassNew;
  for(int b = bStart; b < nb; b++){
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
    // Bueschel-Einfluss: nur Eigenschaften (Hoehe/Yaw/Lean) werden weich
    // zum Bueschel gemischt — Position und Halmzahl bleiben unveraendert
    // (Verschieben wuerde Luecken reißen und den Dichte-Regler ungleichmäßig
    // machen).
    {
      float tdx = bx - cxm, tdz = bz - czm;
      float dc = sqrtf(tdx * tdx + tdz * tdz) / CLUMP;  // 0 = Mitte, ~0.7 = Rand
      float w = 1.0f - c_smoothstep(0.0f, 0.7f, dc);    // Bueschel-Einfluss 1..0
      d[3] *= 1.0f + 0.25f * w * (2.0f * cid - 1.0f);   // Hoehe (symmetrisch um 1)
      float yawC = cid * 6.283f;
      d[7] += w * 0.6f * remainderf(yawC - d[7], 6.283f); // Richtung (kuerzester Weg)
      d[8] *= 0.8f + 0.4f * w;                             // Neigung
    }
    // Birth-Time: Placeholder (0.0), wird in grass_build_finish gesetzt
    //  (erst beim Swap, nicht waehrend des Builds, sonst sind Halme schon "alt")
    d[9] = 0.0f;
    d[10] = 0.0f;   // lebt (Death-Time nur fuer ueberzaehlige/Slider-Halme)
    nGrassNew++;
  }
  int cnt = nGrassNew - start + copied;
  if (cnt > 0) {
    buildOff[izl * CT + ixl] = nGrassNew - cnt;
    buildCnt[izl * CT + ixl] = (uint8_t)cnt;
  }
}


static int out_build_step(int maxCells);   // OUT-Build (Modus 1), s. unten

int grass_build_step(int maxCells){
  // OUT-Build (Modus 1): alte Zell-Tabelle direkt durchgehen
  if (gBuildMode == 1) return out_build_step(maxCells);
  // Diamond-Order (L1-Distanz): Ring dr um das Zentrum, 4*dr Zellen pro Ring
  // (4 Kanten zu je dr Schritten, im Uhrzeigersinn vom oberen Punkt).
  // Bei GRASS_CAP-Ueberlauf fehlen die weitesten Zellen.
  for(int i = 0; i < maxCells; i++){
    if (dr > dmaxR) return 1;
    int x, y;
    if (dr == 0) { x = dcx0; y = dcz0; }
    else {
      int seg = drp / dr, t = drp % dr;
      switch (seg) {
        case 0: x = dcx0 + t;        y = dcz0 - dr + t;      break;  // oben -> rechts
        case 1: x = dcx0 + dr - t;   y = dcz0 + t;           break;  // rechts -> unten
        case 2: x = dcx0 - t;        y = dcz0 + dr - t;      break;  // unten -> links
        default: x = dcx0 - dr + t;  y = dcz0 - t;           break;  // links -> oben
      }
    }
    if (x >= 0 && x < gnx && y >= 0 && y < gnz)
      build_cell(gix0 + x, giz0 + y);
    if (nGrassNew >= GRASS_CAP) return 1;   // Puffer voll: fertig (ferne Zellen weglassen)
    if (++drp >= 4 * dr) { drp = 0; dr++; }
  }
  return dr > dmaxR;
}

// OUT-Build-Step (Modus 1): alte Zell-Tabelle durchgehen, alle Halme als
// sterbend kopieren (d[10]=jetzt -> 2s Fade-out), fertig-verblaeste
// (death > 2.5s zurueck) weglassen. Keine Generierung.
static int out_build_step(int maxCells){
  for (int i = 0; i < maxCells; i++) {
    if (outStepState >= CT * CT) return 1;
    int oi = outStepState++;
    if (cellOff[oi] < 0) continue;
    int cnt = cellCnt[oi];
    int off = cellOff[oi];
    int copied = 0;
    for (int p = 0; p < cnt; p++) {
      const float *src = &grassInst[(off + p) * GRASS_FLOATS];
      if (src[10] > 0.0f && gWorldTime - src[10] >= 2.5f) continue;  // schon weg
      if (nGrassNew >= GRASS_CAP) break;
      float *dst = &buildBuf[nGrassNew * GRASS_FLOATS];
      for (int k = 0; k < GRASS_FLOATS; k++) dst[k] = src[k];
      dst[10] = gWorldTime;   // (neu) sterben: 2s Fade-out
      nGrassNew++;
      copied++;
    }
    if (copied > 0) {
      buildOff[oi] = nGrassNew - copied;
      buildCnt[oi] = (uint8_t)copied;
    }
  }
  return outStepState >= CT * CT;
}

void grass_build_finish(void){
  // Birth-Time setzen (erst beim Swap, nicht waehrend des Builds):
  //  - kopierte Halme (d[9] != 0): alte Birth-Time behalten (Fade laeuft weiter)
  //  - generierte Halme (d[9] == 0): NEU -> blenden ueber 2s ein.
  //    Das sind: der Crescent am Gras-Rand (neue Zellen) + die Zusatz-
  //    halme in Zellen, deren nb durch die Kamerabewegung gestiegen ist
  //    (Teil-Kopie in build_cell) + alle Halme im IN-Build (Slider/Start).
  int nNew = 0, nDying = 0;
  for (int i = 0; i < nGrassNew; i++) {
    float *d = &buildBuf[i * GRASS_FLOATS];
    if (d[9] == 0.0f) { d[9] = gWorldTime; nNew++; }
    if (d[10] > 0.0f) nDying++;
  }
  if (nNew > 0 || nDying > 0)
    printf("Gras-Swap t=%.2f: %d Halme, %d neu, %d sterben (je 2s)\n", gWorldTime, nGrassNew, nNew, nDying);
  if (nGrassNew < 1) nGrassNew = 1;   // defensive: kein 0-Byte-Upload
  pthread_mutex_lock(&gMu);
  hasOldBuild = 1;
  float *tmp = grassInst; grassInst = buildBuf; buildBuf = tmp;
  int *to = cellOff; cellOff = buildOff; buildOff = to;
  uint8_t *tc = cellCnt; cellCnt = buildCnt; buildCnt = tc;
  oldOffX0 = offX0; oldOffZ0 = offZ0;
  old_gnx = gnx; old_gnz = gnz;
  nGrass = nGrassNew;
  gBuildMode = 0;
  gBuilding = 0;
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
void grass_build_request(float cx, float cz, int mode){
  pthread_mutex_lock(&gMu);
  gReqCX = cx; gReqCZ = cz; gReqMode = mode;
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

// DEBUG (F12): aktueller Buffer-Zustand + erwartete Shader-Fade summieren.
void grass_debug_dump(void){
  int nAlive = 0, nFading = 0, nFaded = 0;
  float sumFade = 0.0f;
  for (int i = 0; i < nGrass; i++) {
    const float *d = &grassInst[i * GRASS_FLOATS];
    float death = d[10];
    float fi;
    float age = gWorldTime - d[9];
    float t = age / 2.0f; if (t < 0) t = 0; if (t > 1) t = 1;
    fi = t * t * (3.0f - 2.0f * t);
    if (death <= 0.0f) { nAlive++; sumFade += fi; }
    else {
      float da = (gWorldTime - death) / 2.0f; if (da < 0) da = 0; if (da > 1) da = 1;
      float fo = 1.0f - (da * da * (3.0f - 2.0f * da));
      sumFade += fi * fo;
      if (gWorldTime - death < 2.0f) nFading++; else nFaded++;
    }
  }
  printf("GRAS-DEBUG t=%.2f: %d Halme: %d lebendig, %d fadet, %d verblasst; Summe-ShaderFade=%.0f (%.1f%%)\n",
         gWorldTime, nGrass, nAlive, nFading, nFaded, sumFade, 100.0f * sumFade / (float)nGrass);
  printf("GRAS-DEBUG walk: gix0=%d giz0=%d gnx=%d gnz=%d dmaxR=%d dcx0=%d dcz0=%d bcx=%.2f bcz=%.2f\n",
         gix0, giz0, gnx, gnz, dmaxR, dcx0, dcz0, bcx, bcz);
}

// Build laeuft gerade? (Request wartet oder Worker generiert) — fuer den HUD.
int grass_build_busy(void){
  pthread_mutex_lock(&gMu);
  int b = gReqPending || gBuilding;
  pthread_mutex_unlock(&gMu);
  return b;
}

static void* grass_worker(void* arg){
  (void)arg;
  for(;;){
    pthread_mutex_lock(&gMu);
    while(!gReqPending) pthread_cond_wait(&gCv, &gMu);
    gReqPending = 0;
    float cx = gReqCX, cz = gReqCZ;
    int mode = gReqMode;
    gBuilding = 1;
    pthread_mutex_unlock(&gMu);
    grass_build_begin(cx, cz, mode);
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
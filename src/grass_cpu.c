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
// Halm-Daten (GRASS_FLOATS=10): pos(3) hoehe breite phase schattierung yaw lean grp
//   yaw  = Rotation der Halm-Ebene um Y (sichtbar aus allen Richtungen)
//   lean = Neigung der Spitze in yaw-Richtung (sichtbar von oben)
//   grp  = Index in die Fade-GRUPPEN-Tabelle grassGrp (birth, death). Birth und
//          death sind Fakten pro Build, nicht pro Halm -> nicht mehr pro Halm
//          gespeichert. Fade-Dauer = grassFadeDur (Standard 1s), CPU und Shader
//          nutzen dieselbe.
// Zell-Block im Puffer ist kanonisch [lebende Halme (b-aufsteigend)][sterbende],
// und die Zellentabelle merkt sich den naechsten freien b. Dadurch sind alle
// Copy-/Gen-Entscheidungen Intervalle statt Bitmasken, und die Halme einer Zelle
// sind ueber Builds hinweg dieselbe stabile Teilmenge.
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
static int hasOldBuild = 0;
// Fade-Dauer (CPU und Shader MUESSEN gleich sein, sonst werden sterbende
// Halme aus dem Puffer geworfen, waehrend sie noch einblenden).
// Standard 1s (Nutzer-Wunsch). L-Taste schaltet auf 2s / 10s / 30s: die VM
// laeuft mit 1-2 FPS, ein kurzer Fade ist dort zeitlich nicht aufloesbar.
float grassFadeDur = 1.0f;
// Build-Modi:
//  0 = POS: Positions-Rebuild (Zell-Kopieren; neue Halme faden ein,
//           ueberzaehlige - nb gesunken - faden aus)
//  1 = OUT: Slider-Phase 1 — alle alten Halme sterben (Death-Gruppe dieses
//           Builds), keine Generierung (altes Gras blendet aus)
//  2 = IN:  Slider-Phase 2 / erster Build — alles neu generieren
//           (Birth-Gruppe dieses Builds, neues Gras blendet ein)
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
#define CT 1400   // Raster 1400*0,30m = 420 m Kantenlaenge (war 1000 = 300 m):
// Pro Zelle: Blockanfang, Gesamtzahl, davon LEBENDE (Blockanfang), und der
// naechste freie b-Index. Letzteres macht die Intervall-Markierung moeglich:
// die Generierung setzt bei cellNextB fort statt bei 0 neu anzufangen, also
// sind die Halme einer Zelle ueber Builds hinweg eine stabile Teilmenge.
static int      cellOffA[CT * CT], cellOffB[CT * CT];
static uint8_t  cellCntA[CT * CT], cellCntB[CT * CT];
static uint8_t  cellAliveA[CT * CT], cellAliveB[CT * CT];
static uint8_t  cellNextBA[CT * CT], cellNextBB[CT * CT];
static int     *cellOff = cellOffA;     // Tabelle des alten (sichtbaren) Builds
static uint8_t *cellCnt = cellCntA;
static uint8_t *cellAlive = cellAliveA;
static uint8_t *cellNextB = cellNextBA;
static int     *buildOff = cellOffB;    // Tabelle des aktuellen Builds
static uint8_t *buildCnt = cellCntB;
static uint8_t *buildAlive = cellAliveB;
static uint8_t *buildNextB = cellNextBB;
static int oldOffX0 = 0, oldOffZ0 = 0;  // Tabellen-Origin des alten Builds
static int old_gnx = 0, old_gnz = 0;    // Tabellen-Groesse des alten Builds
static int offX0 = 0, offZ0 = 0;        // Tabellen-Origin des aktuellen Builds

// ================= Fade-GRUPPEN =================
// Birth- und Death-Zeit sind Fakten PRO BUILD, nicht pro Halm: grass_build_
// finish setzte frueher fuer alle Halme eines Builds dieselbe Zeit. Die zwei
// Pro-Halm-Floats waren damit redundant. Jetzt traegt ein Halm nur noch einen
// Gruppen-Index (1 Float), die Zeiten liegen in einer kleinen Tabelle.
//   Gruppe g = (grassGrp[2g] = birth, grassGrp[2g+1] = death); death == 0 lebt
//   Index 0          : unbenutzt
//   Index SETTLED(1) : Fade-in abgeschlossen (birth = -1e9 -> Alpha 1)
//   ab Index 2       : Ring-Allokator
// Ein Build legt EINE Birth-Gruppe an. Sterbende Halme bekommen lazy eine
// Gruppe, die die Birth-Zeit ihrer bisherigen Gruppe uebernimmt und die
// Death-Zeit des Builds traegt (die erst beim Finish bekannt ist).
// Lebensdauer einer Gruppe = eine Fade-Dauer: Fade-in-fertige Halme haengen auf
// SETTLED um, ausfadeierte fallen raus.
// NICHT so gemacht: eine KOMBINIERTE Gruppe (birth,death) pro Halm. Die muesste
// pro (Birth-Gruppe x Build) angelegt werden und waechst kombinatorisch — gemessen
// 24 neue Gruppen pro Build, bei 30s Fade ~2400 gleichzeitig live, die Tabelle
// skaliert mit ihrem Bedarf nicht mit der Groesse. Getrennte Indizes sind strikt
// 2 pro Build, unabhaengig von der Fade-Dauer.
#define GRASS_GRP_SETTLED 1
float grassGrp[GRASS_NGRP * 2];
// Ein Halm traegt beide Indizes gepackt in EINEM Float:
//   wert = birthGrp + deathGrp * GRASS_NGRP   (deathGrp 0 = lebt)
// Bei NGRP=512 sind das 2*9 Bit — exakt in einem float darstellbar.
static int gGrpNext = 2, gNewBirthGrp = GRASS_GRP_SETTLED, gNewDieGrp = 0;
static int gCntNew = 0, gCntDying = 0;   // nur fuer die Swap-Meldung
static int grpAlloc(void){
  int i = gGrpNext++;
  if (gGrpNext < 2 || gGrpNext >= GRASS_NGRP) gGrpNext = 2;   // Ring
  return i;
}
static int  gBirthI(float v){ return (int)v % GRASS_NGRP; }
static int  gDeathI(float v){ return (int)v / GRASS_NGRP; }
static float gPack(int b, int d){ return (float)b + (float)d * (float)GRASS_NGRP; }
static float gBirthT(float v){ return grassGrp[gBirthI(v)*2]; }
static float gDeathT(float v){ int d = gDeathI(v); return d ? grassGrp[d*2+1] : 0.0f; }

#define GSTEP 0.30f
#define GRASS_NBMAX 128.0f   // Obergrenze Halme/Zelle (uint8_t-Zellen bis 255)
#define CLUMP 1.2f   // Bueschel-Grid (Schritt 4): naechster Mittelpunkt in 3x3-Zellen
// Dichte-Abschneidschwelle der Generierung (= Build-Radius, Bisektion).
// WICHTIG: die CPU-Dichte faellt glatt auf 0 bei grassMaxDist; diese Schwelle
// schneidet frueher ab und erzeugt damit eine HARTE Kante (Halmzahl springt
// von 32*dens*0.7 auf 0). Liegt die Kante innerhalb des Shader-LOD-Fadebereichs
// (grassMaxDist-12 .. grassMaxDist), ist sie VOLLSICHTBAR: beim Build-Zentrums-
// hop (Freeze-AUS, Rueckwaertsfahrt) wandert sie und ein Band neuer Halme
// fadet voll sichtbar ein (Nutzer: „ploetzlich erscheinende Halme an der
// Graskante“). 0.09 lag bei 66,7m mit lod=1.00 (voll sichtbar). 0.01 legt sie
// auf ~75,8m mit lod=0.29 -> Kante liegt im gedaempften Bereich.
#define GRASS_DENS_CUT 0.01f
// DEBUG-Zweig: Laufzeit-Schalter fuer die Kante (K-Taste). 0 = kein Abschneiden,
// Feld endet weich bei grassMaxDist.
static float gDensCut = GRASS_DENS_CUT;
// Build-Radius = Dichte-Cutoff (wo dens auf gDensCut faellt), NICHT
// grassMaxDist+20: auBerhalb generiert build_cell keine Halme, der alte
// +20m-Margin hat nur Build-Zeit gekostet. Cutoff = Loesung von
// dens(d) = gDensCut (Bisektion, pro grassMaxDist gecacht).
static float gDCut = 0.0f, gDCutMD = -1.0f;
void grassSetDensCut(float v){
  if (v == gDensCut) return;
  gDensCut = v;
  gDCutMD = -1.0f;   // Bisektion-Cache entwerten
}
static float dens_at(float dist);   // Profil unten (LUT), hier fuer g_half noetig
static float g_half(void){
  float md = grassMaxDist;
  if (md != gDCutMD) {
    float lo = 8.0f, hi = md;
    for (int i = 0; i < 30; i++) {
      float mid = (lo + hi) * 0.5f;
      float dens = dens_at(mid);   // EINE Profil-Quelle (war: Formel dupliziert)
      // Schwelle GRASS_DENS_CUT: Kante, an der die Generierung abschneidet.
      // Muss die SAME Schwelle sein wie der dens-Test im Kopierpfad und im
      // Generierungspfad, sonst entstehen Luecken/harte Kanten (s. oben).
      if (dens >= gDensCut) lo = mid; else hi = mid;
    }
    gDCut = (lo + hi) * 0.5f;
    gDCutMD = md;
  }
  return gDCut;
}

// Dichte-LUT: dens = f(dist) ist radial -> Tabelle statt powf (macht den
// Zell-Kopier-Pfad deutlich schneller). 0.5m-Resolution, pro grassMaxDist
// neu befuellt (Slider).
// 2026-10-10: 480 Eintraege = 0..240 m reichten genau bis zum alten Slider-Max.
// Oberhalb gab die LUT den Wert von 239,5 m zurueck (nicht 0), die Bisektion der
// Build-Kante fand keinen Null-Durchgang, und das Dichte-Profil war fuer jede
// Zelle jenseits 240 m falsch. 1200 Eintraege = 0..600 m (4,8 KB).
#define DENS_LUT_N 1200   // 0..600m
static float densLut[DENS_LUT_N];
static float densLutMD = -1.0f;
// Dichte-Profil. Einheit: 1.0 = heutige Volldichte (32 Halme/Zelle VOR den
// Verwuerfen durch Flecken-/Steilhang-/Wasser-Tests; die werfen gemessen ~50 %
// raus, kalibriert 0.497).
//   Sockel  1/(1+(r/R1)^2)   flach bis ~R1, danach ~1/r^2. Der 1/r^2-Schwanz
//          ist der Grund, warum die Gesamtzahl nur LOGARITHMISCH mit der
//          Reichweite waechst: N = pi*n0*R1^2*ln(1+(R/R1)^2).
//   Boost   3/(1+(r/RB)^4)   4x im engsten Nahfeld, sehr schnell weg
//          (r=1: 3.00, r=2: 1.50, r=3: 0.37, r=4: 0.012) — kostet nur 0,6 %
//          des Halmebudgets (5215 Halme innerhalb 2 m).
//   Ausschnitt (1-smoothstep(md-EDGEW, md, r)) faellt exakt auf 0 bei md.
//     EDGEW MUSS das selbe Band sein wie die LOD-Daempfung im Shader
//     (grass.vert: lod = 1-smoothstep(uMaxDist-12, uMaxDist, d)). Liegt die
//     CPU-Kante ausserhalb dieses Bands, schneidet die Generierung bei lod~1.0
//     -> harte, voll sichtbare Graskante (der Fehler, den Runde 7 behoben hat).
//     Mit EDGEW=12 liegt die Kante bei md-3..md-8 und damit lod<0.3.
// Bewusst NICHT 'konstante Pixeldichte': die verlangt n*ss^2 = konst, also bei
// 1/r^2-Dichte einen Groessenwuchs ss ~ r (bei 120m: 4,3m hohe Halme).
#define DENS_R1 27.0f
#define DENS_RB 2.0f
#define DENS_EDGEW 12.0f   // = LOD-Band in grass.vert
#define DENS_P 2.0f        // Sockel-Exponent: 1/r^2. Vorher 1/r^3 -> die Halmdichte
                           // fiel mit der Reichweite viel schneller als ihre Bild-
                           // bedeckung, ab ~50 m blieb fast nichts uebrig und die
                           // Einfangtextur musste die ganze Ferne fuellen (Nutzer:
                           // 'Gras deutlich weiter zeichnen'). 1/r^2 haelt die
                           // Bildbedeckung naeherungsweise konstant.
static float dens_at(float dist){
  float md = grassMaxDist;
  if (md != densLutMD) {
    for (int i = 0; i < DENS_LUT_N; i++){
      float r = i * 0.5f;
      float s = r / DENS_R1, b = r / DENS_RB;
      float sockel = 1.0f / (1.0f + powf(s, DENS_P));
      float boost  = 3.0f / (1.0f + b*b*b*b);
      densLut[i] = (sockel + boost) * (1.0f - c_smoothstep(md - DENS_EDGEW, md, r));
    }
    densLutMD = md;
  }
  // LINEAR interpoliert. Vorher stand hier (int)(dist*2.0f) ohne Interpolation
  // -> die Dichte war eine Treppe mit 0,5m-Stufen, also konzentrische Ringe.
  float fi = dist * 2.0f;
  int i = (int)fi;
  if (i >= DENS_LUT_N - 1) return densLut[DENS_LUT_N - 1];
  float t = fi - (float)i;
  return densLut[i] + (densLut[i + 1] - densLut[i]) * t;
}

// Sortierte Walk-Ordnung (DEBUG-Vergleich, I-Taste): Zellen von FERN nach NAH
// entlang der Blickrichtung. Der Puffer ist dann global hinten->vorn sortiert,
// die Zell-Bloeecke bleiben aber zusammenhaengend (cellOff/cellCnt bleiben gueltig).
// Kernaendpunkt: Sortieren nach Projektion auf eine FESTE Achse ist
// translationsinvariant — eine Kameraverschiebung aendert den Schluessel aller
// Zellen um dieselbe Konstante, die Reihenfolge bleibt also korrekt. Nur eine
// Drehung zwischen zwei Builds (0,3..1s) verschlechtert sie leicht.
static float gViewX = 1.0f, gViewZ = 0.0f;
static int gSortedOrder = 0;
#define NSLAB 512
static int walkList[CT * CT], walkN = 0, walkPos = 0;
static int slabCnt[NSLAB], slabStart[NSLAB + 1];
void grassSetViewDir(float x, float z){ gViewX = x; gViewZ = z; }
void grassSetSortOrder(int on){ gSortedOrder = on; }
int  grassSortedOrderActive(void){ return gSortedOrder; }
static float slabKey(int ix, int iz){
  return ((float)ix - bcx / GSTEP) * gViewX * GSTEP + ((float)iz - bcz / GSTEP) * gViewZ * GSTEP;
}
static void build_walk_sorted(void){
  float kmin = 1e30f, kmax = -1e30f;
  for (int y = 0; y < gnz; y++)
    for (int x = 0; x < gnx; x++){
      float k = slabKey(gix0 + x, giz0 + y);
      if (k < kmin) kmin = k;
      if (k > kmax) kmax = k;
    }
  float span = kmax - kmin; if (span < 1e-3f) span = 1e-3f;
  float inv = (float)(NSLAB - 1) / span;
  for (int s = 0; s < NSLAB; s++) slabCnt[s] = 0;
  for (int y = 0; y < gnz; y++)
    for (int x = 0; x < gnx; x++)
      slabCnt[(int)((slabKey(gix0 + x, giz0 + y) - kmin) * inv)]++;
  // Rang 0 = fernstes Slab (grosses k), Rang NSLAB-1 = naechstes
  slabStart[0] = 0;
  for (int r = 0; r < NSLAB; r++) slabStart[r + 1] = slabStart[r] + slabCnt[NSLAB - 1 - r];
  int cur[NSLAB];
  for (int r = 0; r < NSLAB; r++) cur[r] = slabStart[r];
  for (int y = 0; y < gnz; y++)
    for (int x = 0; x < gnx; x++){
      int s = (int)((slabKey(gix0 + x, giz0 + y) - kmin) * inv);
      int r = NSLAB - 1 - s;
      walkList[cur[r]++] = y * gnx + x;      // lokaler Index ins Walk-Quadrat
    }
  walkN = gnx * gnz;
  walkPos = 0;
}

void grass_build_begin(float cx, float cz, int mode){
  // hasOldBuild wird erst in grass_build_finish gesetzt (erster Build =
  // alle Halme sofort sichtbar, kein Vergleich gegen das (0,0)-Zentrum).
  bcx = cx; bcz = cz;
  gBuildMode = mode;
  nGrassNew = 0;
  gCntNew = 0; gCntDying = 0;
  buildBuf = (grassInst == gA) ? gB : gA;
  // Build-Tabelle leeren: auBerhalb des Build-Quadrats darf nichts altes
  // stehen (das Zell-Lookup liest cellOff[oi] >= 0 als "Zelle existiert").
  for (int i = 0; i < CT * CT; i++) {
    buildOff[i] = -1; buildCnt[i] = 0; buildAlive[i] = 0; buildNextB[i] = 0;
  }
  // Fade-Gruppen dieses Builds: eine Birth-Gruppe fuer alle neu erzeugten
  // Halme, und pro bisheriger Gruppe lazy eine "stirbt jetzt"-Gruppe. Die
  // Zeiten selbst entstehen erst in grass_build_finish (Swap-Punkt) — der Build
  // laeuft 100..900ms und gWorldTime laeuft dabei weiter. Genau dieser
  // Unterschied (Zeit beim Build statt beim Swap) war der alte Bug a4ddeb9;
  // mit Gruppen ist er nicht mehr darstellbar.
  if (grassGrp[GRASS_GRP_SETTLED*2+0] == 0.0f) {
    grassGrp[GRASS_GRP_SETTLED*2+0] = -1e9f;   // Fade-in immer fertig
    grassGrp[GRASS_GRP_SETTLED*2+1] = 0.0f;
  }
  gNewBirthGrp = grpAlloc();
  gNewDieGrp   = grpAlloc();   // EINE Death-Gruppe fuer diesen Build
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
    // Clamp: Quadrat liegt ZENTRIERT auf der Kamera (nicht: kleine Kante
    // behalten). Kante-behalten pinnte die hintere Kante am Startpunkt —
    // bei Fahrt in Richtung wachsender Indizes (+x/+z) wurde die FRONT-
    // KANTE (neue Generierung) nach ~77m abgeschnitten -> "kein neues
    // Gras mehr" (Issue B, VM-bestaetigt). Sobald der Clamp greift, wird
    // pro Build weiter zentriert (Flags): Front immer volle 66,7m; der
    // verlorene Rand-Schimmer (~1m) poppt bei ~105m Kamera-Distanz =
    // jenseits des MD=80-Culls -> unsichtbar. Ohne Flags waere das
    // Quadrat 128m fixiert und sprae dann (38m Fade-in-Band an der Kante).
    static int clampedX = 0, clampedZ = 0;
    if (gnx > CT || clampedX) { gnx = CT; gix0 = (int)floorf(cx / GSTEP) - CT / 2; clampedX = 1; }
    if (gnz > CT || clampedZ) { gnz = CT; giz0 = (int)floorf(cz / GSTEP) - CT / 2; clampedZ = 1; }
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
  if (gSortedOrder) build_walk_sorted();
}


// Pflanzen-Typen (Arten, Blumen, Schilf)
// Typen: 0 Wiese (Standard) · 1 Hochgras · 2 Lavendel · 3 Gaensebluemchen
//        4 Schilf/Rohricht
// WICHTIG: CPU-seitig existiert nur das Schilf-Feld c_reedW (Ufer-Gate).
// Hoehe/Breite/Neigung je Art rechnet der SHADER (grass.vert). Grund: ein
// CPU-/Shader-Vergleich der Art lieferte 3,3 % Abweichungen (Rundung/FMA in
// fbm+hash, Test /tmp/specmatch.c). Hatte die CPU die Hoehe nach Art gesetzt,
// haetten manche Halme Hoehe und Farbe von UNTERSCHIEDLICHEN Arten bekommen.
// So existiert die Art fuer alles Sichtbare genau EINMAL, im Shader.
// Die Felder sind weiche fbm-Felder -> Regionen ohne harte Kanten. Botanisch:
// Schilf in Rieden am/ufer (dh = Hoehe ueber Wasserlinie), Lavendel auf
// trockenen sonnigen Haengen, Hochgras und Gaensebluemchen in feuchteren Wiesen.
static float c_rgf(float px, float py){
  float s = 0.5f * c_vnoise(px, py);
  s += 0.25f * c_vnoise(px * 2.03f + 17.1f, py * 2.03f + 9.2f);
  return s * 1.3333f;
}
static float c_reedW(float x, float z, float y){
  float dh = y - WATER_Y;
  if(dh > 0.45f) return 0.0f;
  float patch = c_rgf(x * 0.05f + 3.7f, z * 0.05f);
  return (1.0f - c_smoothstep(0.05f, 0.45f, dh)) * c_smoothstep(0.42f, 0.60f, patch);
}

// Halm-Anzahl einer Zelle — EINE Formel, immer am ZELLZENTRUM.
// Frueher rechnete der Kopierpfad am Zellzentrum, die Generierung am gestreuten
// Punkt (px,pz) -> ±1-Differenz, und das war die Quelle des Overlap-Bugs
// (alter + neuer Halm an exakt derselben Stelle, Fix 13a0dbf).
static int cell_nb(int ix, int iz, float dist){
  float dens = dens_at(dist);
  if (dens < gDensCut) return 0;
  float h4 = c_hash12(ix * 0.271f, iz * 0.711f);
  float nbf = 32.0f * dens * (0.4f + 0.6f * h4) * grassDensityScale;
  if (nbf > GRASS_NBMAX) nbf = GRASS_NBMAX;   // war 32; Profil geht bis ~4x vorn
  int nb = (int)nbf;
  if (c_hash12(ix * 0.917f, iz * 0.311f) < nbf - (float)nb) nb++;   // stochastisch
  return nb;
}

// Erzeugt bis zu nWant Halme der Zelle, fortlaufend ab b = bStart.
// b wird NUR vorwaerts fortgesetzt (bStart kommt aus der Zellentabelle), die
// Halme einer Zelle sind also immer "die ersten nb gueltigen b" — eine ueber
// Builds stabile, flaechig streuende Teilmenge. Das ist die Intervall-Marke-
// rung: kein copiedMask, kein b-Index pro Halm.
// *nextBOut = naechster freier b (im Folge-Build fortsetzbar).
// Rueckgabe: Anzahl erzeugter Halme. Alle Halme bekommen die Birth-Gruppe
// dieses Builds; ihre Zeit schreibt grass_build_finish (Swap-Punkt).
static int gen_blades(int ix, int iz, float x, int bStart, int nWant, int *nextBOut){
  float z = iz * GSTEP;
  float h1 = c_hash12(ix * 0.123f, iz * 0.457f);
  float h2 = c_hash12(ix * 0.457f, iz * 0.123f);
  float h3 = c_hash12(ix * 0.711f, iz * 0.271f);
  float px = x + (h1 - 0.5f) * 0.5f * GSTEP;
  float pz = z + (h2 - 0.5f) * 0.5f * GSTEP;
  float h = c_terrainH(px, pz);
  // Schilf/Rohricht steht flach im Wasser oder direkt an der Wasserlinie, daher
  // ist die Untergrenze WATER_Y-0.12 (vorher WATER_Y+0.10: Uferzone fehlte ganz).
  if(h < WATER_Y - 0.12f || h > 7.5f) return 0;  // kein tiefes Wasser, keine Gipfel
  // Unter der Wasserlinie nur Schilf (Riede). Diese Pruefung gehoert auf ZELL-
  // EBENE: pro Halm waere sie ein weiteres fbm-Feld, und weil verworfene b die
  // b-Suche bis 255 laufen lassen, war der 1m-POS-Build 9x langsamer (186 ->
  // 1649 ms, gemessen). Das Rieden-Feld hat ~20 m Wellenlaenge, eine Zelle ist
  // 0,3 m — der Unterschied Zellmitte vs. Halm ist visuell irrelevant.
  if(h < WATER_Y + 0.10f && c_reedW(px, pz, h - 0.03f) < 0.30f) return 0;
  if(fabsf(c_terrainH(px + 0.4f, pz) - h) > 0.25f ||
     fabsf(c_terrainH(px, pz + 0.4f) - h) > 0.25f) return 0;  // Steilhang
  float cl = c_fbm(px * 0.12f, pz * 0.12f);       // Gras-Flecken
  if(h3 > cl + 0.50f) return 0;                  // Flecken
  // Clumping (Schritt 4): naechster Bueschel-Mittelpunkt (1.2m-Grid, 3x3).
  // Einmal pro ZELLE gesucht (nicht pro Halm: 18 Sin-Hashes pro Halm waeren zu
  // teuer fuer 1m-Rebuilds; Halm-Offsets in der Zelle sind ±0.14m = 12% von
  // CLUMP, visuell vernachlaessigbar).
  int cix = (int)floorf(px / CLUMP), ciz = (int)floorf(pz / CLUMP);
  float cxm = px, czm = pz, cid = 0.0f;
  float best = 1e9f;
  for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++) {
      float hx = c_hash12((cix + i) * 0.731f, (ciz + j) * 0.219f);
      float hz = c_hash12((cix + i) * 0.219f, (ciz + j) * 0.731f);
      float mx = (cix + i + hx) * CLUMP, mz = (ciz + j + hz) * CLUMP;
      float d2 = (px - mx) * (px - mx) + (pz - mz) * (pz - mz);
      if (d2 < best) { best = d2; cxm = mx; czm = mz; cid = c_hash12(mx, mz); }
    }
  int made = 0;
  int b = bStart;
  for (; made < nWant && b < 255; b++){
    if(nGrassNew >= GRASS_CAP) break;
    float b1 = c_hash12(ix * 1.13f + b, iz * 1.71f + b);
    float b2 = c_hash12(ix * 1.71f + b, iz * 1.13f + b);
    float b3 = c_hash12(ix * 2.13f + b, iz * 2.71f + b);
    float b4 = c_hash12(ix * 2.71f + b, iz * 2.13f + b);
    float b5 = c_hash12(ix * 3.13f + b, iz * 3.71f + b);
    float b6 = c_hash12(ix * 3.71f + b, iz * 3.13f + b);
    float bx = px + (b1 - 0.5f) * 0.28f;
    float bz = pz + (b2 - 0.5f) * 0.28f;
    float bh = c_terrainH(bx, bz);
    if(bh < WATER_Y - 0.12f) continue;   // dieser b faellt aus (tiefes Wasser)
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
    d[GRASS_FLOATS-1] = gPack(gNewBirthGrp, 0);   // Birth-Gruppe dieses Builds
    nGrassNew++; made++; gCntNew++;
  }
  *nextBOut = b;
  return made;
}

// Eine Zelle: Kopieren (Intervalle) und/oder generieren.
// Zell-Block im Puffer, kanonische Reihenfolge:
//   [ lebende Halme (b-aufsteigend) ][ sterbende Halme ]
// Darum sind alle Entscheidungen Intervalle, keine Masken:
//   - nb gesunken   -> [keep, oalive) sterben neu (keep = min(nb, oalive))
//   - nb gestiegen  -> die naechsten (nb - keep) gueltigen b ab cellNextB neu
//   - schon sterbend -> mitnehmen, bis der Fade zu Ende ist, dann weg
// Ein Halm, der am Leben bleibt, behaelt seine Birth-Gruppe; ist sein Fade-in
// fertig, haengt er auf die Settled-Gruppe um (haelt die Gruppentabelle klein).
static void build_cell(int ix, int iz){
  float x = ix * GSTEP, z = iz * GSTEP;
  int ti = (iz - giz0) * CT + (ix - gix0);
  buildOff[ti] = -1; buildCnt[ti] = 0; buildAlive[ti] = 0; buildNextB[ti] = 0;
  // AuBerhalb des NEUEN Build-Radius: nur Zellen mit altem Gras verarbeiten
  // (Kanten-Sichel: ihre Halme faden aus statt sofort zu pochen; der Walk-
  // Radius deckt die Union der alten/neuen Disks ab, s. begin).
  float dx = x - bcx, dz = z - bcz;
  float dist = sqrtf(dx * dx + dz * dz);
  int outsideNew = dist > g_half();
  int oldIdx = -1;
  if (gBuildMode == 0 && hasOldBuild) {
    int ox = ix - oldOffX0, oz = iz - oldOffZ0;
    if (ox >= 0 && ox < CT && oz >= 0 && oz < CT) {
      int oi = oz * CT + ox;
      if (cellOff[oi] >= 0) oldIdx = oi;
    }
  }
  if (outsideNew && oldIdx < 0) return;   // neue Zelle ausserhalb: nichts zu tun
  int start = nGrassNew;
  int alive = 0, nextB = 0;
  if (oldIdx >= 0) {
    int nb = cell_nb(ix, iz, dist);
    int ostart = cellOff[oldIdx], oalive = cellAlive[oldIdx];
    int ototal = cellCnt[oldIdx], onextB = cellNextB[oldIdx];
    int keep = (oalive < nb) ? oalive : nb;
    // 1) lebende Halme behalten (Blockanfang). Birth-Gruppe behalten -> ein
    //    laufender Fade-in geht nahtlos weiter.
    for (int p = 0; p < keep; p++){
      if (nGrassNew >= GRASS_CAP) break;
      const float *src = &grassInst[(ostart + p) * GRASS_FLOATS];
      float *dst = &buildBuf[nGrassNew * GRASS_FLOATS];
      for (int k = 0; k < GRASS_FLOATS; k++) dst[k] = src[k];
      int gb = gBirthI(src[GRASS_FLOATS-1]);
      // Fade-in fertig -> auf Settled-Gruppe umhaengen. Konservativ: gWorldTime
      // ist hier FRUEHER als der Swap, also wird nie zu frueh settled.
      if (gWorldTime - grassGrp[gb*2] >= grassFadeDur) gb = GRASS_GRP_SETTLED;
      dst[GRASS_FLOATS-1] = gPack(gb, 0);
      nGrassNew++; alive++;
    }
    nextB = onextB;
    // 2) fehlende Halme erzeugen — Intervall ab cellNextB, VOR den sterbenden,
    //    damit der Block kanonisch [lebend][sterbend] bleibt.
    if (nb > alive && !outsideNew)
      alive += gen_blades(ix, iz, x, onextB, nb - alive, &nextB);
    // 3) ueberzaehlige lebende: sterben in DIESEM Build (Death-Zeit = Swap)
    for (int p = keep; p < oalive; p++){
      if (nGrassNew >= GRASS_CAP) break;
      const float *src = &grassInst[(ostart + p) * GRASS_FLOATS];
      float *dst = &buildBuf[nGrassNew * GRASS_FLOATS];
      for (int k = 0; k < GRASS_FLOATS; k++) dst[k] = src[k];
      dst[GRASS_FLOATS-1] = gPack(gBirthI(src[GRASS_FLOATS-1]), gNewDieGrp);
      nGrassNew++; gCntDying++;
    }
    // 4) schon sterbende: mitnehmen, bis der Fade zu Ende ist
    for (int p = oalive; p < ototal; p++){
      const float *src = &grassInst[(ostart + p) * GRASS_FLOATS];
      float death = gDeathT(src[GRASS_FLOATS-1]);
      if (death > 0.0f && gWorldTime - death >= grassFadeDur * 1.25f) continue;
      if (nGrassNew >= GRASS_CAP) break;
      float *dst = &buildBuf[nGrassNew * GRASS_FLOATS];
      for (int k = 0; k < GRASS_FLOATS; k++) dst[k] = src[k];
      nGrassNew++;
    }
  } else {
    // neue Zelle (oder IN-Build): alles generieren, b ab 0
    int nb = cell_nb(ix, iz, dist);
    if (nb > 0) alive = gen_blades(ix, iz, x, 0, nb, &nextB);
  }
  int cnt = nGrassNew - start;
  if (cnt > 0) {
    buildOff[ti] = start;
    buildCnt[ti] = (uint8_t)cnt;
    buildAlive[ti] = (uint8_t)alive;
    buildNextB[ti] = (uint8_t)nextB;
  }
}


static int out_build_step(int maxCells);   // OUT-Build (Modus 1), s. unten

int grass_build_step(int maxCells){
  // OUT-Build (Modus 1): alte Zell-Tabelle direkt durchgehen
  if (gBuildMode == 1) return out_build_step(maxCells);
  // Sortierte Ordnung: Zellen fern->nah entlang der Blickrichtung. Der Puffer
  // ist dann von vorn nach hinten sortiert, gezeichnet werden muss von Index 0
  // aufwaerts (grassSortedOrderActive sagt main.c Bescheid).
  if (gSortedOrder) {
    for (int i = 0; i < maxCells; i++){
      if (walkPos >= walkN) return 1;
      int c = walkList[walkPos++];
      build_cell(gix0 + (c % gnx), giz0 + (c / gnx));
      if (nGrassNew >= GRASS_CAP) return 1;
    }
    return 0;
  }
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
    int off = cellOff[oi], oalive = cellAlive[oi], ototal = cellCnt[oi];
    int start = nGrassNew;
    // Block ist kanonisch [lebend][sterbend]: die lebenden sterben alle in
    // diesem Build, die sterbenden wandern mit, bis ihr Fade durch ist.
    for (int p = 0; p < ototal; p++) {
      const float *src = &grassInst[(off + p) * GRASS_FLOATS];
      float v = src[GRASS_FLOATS-1];
      if (p >= oalive) {
        float death = gDeathT(v);
        if (death > 0.0f && gWorldTime - death >= grassFadeDur * 1.25f) continue;
      } else {
        v = gPack(gBirthI(v), gNewDieGrp);
      }
      if (nGrassNew >= GRASS_CAP) break;
      float *dst = &buildBuf[nGrassNew * GRASS_FLOATS];
      for (int k = 0; k < GRASS_FLOATS; k++) dst[k] = src[k];
      dst[GRASS_FLOATS-1] = v;
      nGrassNew++;
      if (p < oalive) gCntDying++;
    }
    int cnt = nGrassNew - start;
    if (cnt > 0) {
      buildOff[oi] = start;
      buildCnt[oi] = (uint8_t)cnt;
      buildAlive[oi] = 0;                    // nichts lebt mehr
      buildNextB[oi] = cellNextB[oi];        // b-Fortfuehrung erhalten
    }
  }
  return outStepState >= CT * CT;
}

void grass_build_finish(void){
  // Fade-Gruppen: die ZEITEN entstehen erst hier (Swap-Punkt). Die Halme
  // tragen seit dem Umbau nur noch den Gruppen-Index, deshalb ist der fruehere
  // Bug (Birth-/Death-Time waehrend des laufenden Builds gesetzt, a4ddeb9) nicht
  // mehr moeglich. Vorher: Vollpass ueber alle Halme, um birth/death nachzu-
  // ziehen — der entfaellt, es zaehlen nur die Gruppen der beiden Builds.
  grassGrp[gNewBirthGrp*2+0] = gWorldTime;
  grassGrp[gNewBirthGrp*2+1] = 0.0f;
  grassGrp[gNewDieGrp*2+0]   = 0.0f;      // Birth wird von einer Death-Gruppe nie gelesen
  grassGrp[gNewDieGrp*2+1]   = gWorldTime;
  if (gCntNew > 0 || gCntDying > 0)
    printf("Gras-Swap t=%.2f: %d Halme, %d neu, %d sterben (je %.0fs)\n",
           gWorldTime, nGrassNew, gCntNew, gCntDying, grassFadeDur);
  if (nGrassNew < 1) nGrassNew = 1;   // defensive: kein 0-Byte-Upload
  pthread_mutex_lock(&gMu);
  hasOldBuild = 1;
  float *tmp = grassInst; grassInst = buildBuf; buildBuf = tmp;
  int *to = cellOff; cellOff = buildOff; buildOff = to;
  uint8_t *tc = cellCnt; cellCnt = buildCnt; buildCnt = tc;
  tc = cellAlive; cellAlive = buildAlive; buildAlive = tc;
  tc = cellNextB; cellNextB = buildNextB; buildNextB = tc;
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
    float v = d[GRASS_FLOATS-1];
    float death = gDeathT(v);
    float fi;
    float age = gWorldTime - gBirthT(v);
    float t = age / grassFadeDur; if (t < 0) t = 0; if (t > 1) t = 1;
    fi = t * t * (3.0f - 2.0f * t);
    if (death <= 0.0f) { nAlive++; sumFade += fi; }
    else {
      float da = (gWorldTime - death) / grassFadeDur; if (da < 0) da = 0; if (da > 1) da = 1;
      float fo = 1.0f - (da * da * (3.0f - 2.0f * da));
      sumFade += fi * fo;
      if (gWorldTime - death < grassFadeDur * 2.0f) nFading++; else nFaded++;
    }
  }
  printf("GRAS-DEBUG t=%.2f: %d Halme: %d lebendig, %d fadet, %d verblasst; Summe-ShaderFade=%.0f (%.1f%%)\n",
         gWorldTime, nGrass, nAlive, nFading, nFaded, sumFade, 100.0f * sumFade / (float)nGrass);
  printf("GRAS-DEBUG walk: gix0=%d giz0=%d gnx=%d gnz=%d dmaxR=%d dcx0=%d dcz0=%d bcx=%.2f bcz=%.2f\n",
         gix0, giz0, gnx, gnz, dmaxR, dcx0, dcz0, bcx, bcz);
}

// DEBUG (Taste n/m): Halme in 10m Umkreis um einen Punkt (Position, Hoehe,
// Birth, Death) — fuer den Issue-A-Vergleich vor/nach Build. Der zweite
// Dump vergleicht mit dem ersten (gleiche Positionen? geaenderte Werte?).
static float gDbgX = 0, gDbgZ = 0;
void grass_debug_set_point(float x, float z){ gDbgX = x; gDbgZ = z; }
#define RDBG_N 20000
#define RDBG_SLOTS 3
static float rdbgH[RDBG_SLOTS][RDBG_N], rdbgB[RDBG_SLOTS][RDBG_N], rdbgD[RDBG_SLOTS][RDBG_N];
static int   rdbgN[RDBG_SLOTS] = {0, 0, 0};
static long  rdbgPx[RDBG_SLOTS][RDBG_N], rdbgPz[RDBG_SLOTS][RDBG_N];
void grass_debug_region_dump(int compare, int slot){
  if (slot < 0 || slot >= RDBG_SLOTS) slot = 0;
  pthread_mutex_lock(&gMu);
  int n = 0, nDying = 0, nNew = 0;
  float hMin = 1e9f, hMax = -1e9f;
  long px[RDBG_N], pz[RDBG_N];
  float h[RDBG_N], b[RDBG_N], d[RDBG_N];
  for (int i = 0; i < nGrass; i++) {
    const float *dd = &grassInst[i * GRASS_FLOATS];
    float dx = dd[0] - gDbgX, dz = dd[2] - gDbgZ;
    if (dx * dx + dz * dz > 100.0f) continue;
    if (n >= RDBG_N) continue;
    px[n] = (long)(dd[0] * 1000.0f); pz[n] = (long)(dd[2] * 1000.0f);
    h[n] = dd[3];
    { float v = dd[GRASS_FLOATS-1];
      b[n] = gBirthT(v); d[n] = gDeathT(v); n++;
      if (gDeathT(v) > 0.0f) nDying++;
      if (gWorldTime - gBirthT(v) < grassFadeDur * 3.0f) nNew++; }
    if (dd[3] < hMin) hMin = dd[3];
    if (dd[3] > hMax) hMax = dd[3];
  }
  printf("GRAS-REGION t=%.2f: %d Halme in 10m um (%.1f, %.1f): %d sterben, %d neu(<3s), h %.3f..%.3f\n",
         gWorldTime, n, gDbgX, gDbgZ, nDying, nNew, hMin, hMax);
  if (compare && rdbgN[slot] > 0) {
    // Vergleich mit vorherigem Dump (Position auf 1mm)
    int nMatch = 0, nGone = 0, nAppeared = 0, nHChg = 0, nBChg = 0, nDChg = 0;
    for (int i = 0; i < n; i++) {
      int found = -1;
      for (int j = 0; j < rdbgN[slot]; j++)
        if (rdbgPx[slot][j] == px[i] && rdbgPz[slot][j] == pz[i]) { found = j; break; }
      if (found < 0) { nAppeared++; continue; }
      nMatch++;
      if (fabsf(h[i] - rdbgH[slot][found]) > 0.01f) nHChg++;
      if (fabsf(b[i] - rdbgB[slot][found]) > 0.01f) nBChg++;
      if (fabsf(d[i] - rdbgD[slot][found]) > 0.01f) nDChg++;
    }
    for (int j = 0; j < rdbgN[slot]; j++) {
      int found = 0;
      for (int i = 0; i < n; i++)
        if (rdbgPx[slot][j] == px[i] && rdbgPz[slot][j] == pz[i]) { found = 1; break; }
      if (!found) nGone++;
    }
    printf("GRAS-REGION DIFF (vs. vorher): %d match, %d WEG (ohne Tod), %d NEU, Hoehe-geaendert=%d, Birth-geaendert=%d, Death-geaendert=%d\n",
           nMatch, nGone, nAppeared, nHChg, nBChg, nDChg);
  }
  for (int i = 0; i < n && i < 25; i++)
    printf("GRAS-REGION %3d: pos=(%.2f,%.2f) h=%.3f birth=%.2f death=%.2f\n",
           i + 1, px[i] / 1000.0f, pz[i] / 1000.0f, h[i], b[i], d[i]);
  for (int i = 0; i < n; i++) { rdbgPx[slot][i] = px[i]; rdbgPz[slot][i] = pz[i]; rdbgH[slot][i] = h[i]; rdbgB[slot][i] = b[i]; rdbgD[slot][i] = d[i]; }
  rdbgN[slot] = n;
  pthread_mutex_unlock(&gMu);
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
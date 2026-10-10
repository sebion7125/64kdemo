// Wirtstest OHNE RENDERING: derselbe Baum, verschiedene Build-Entfernungen.
//
// Anlass (Nutzer 2026-10-10): "mag sein, dass deine Huellen gebaut werden, aber ich
// sehe nur sehr wenige davon ... und die nackten Baeume in der Entfernung werden
// geloescht, wenn ich mich auf sie zubewege". Im Ein-Baum-Renderlauf zeigte sich
// dieselbe Baum mit 94, 45, 104, 95 und 41 Huellenzellen — also haengte die
// Geometrie von der Build-Entfernung ab. Das ist ohne GL messbar: trees_cpu.c ist
// reines C (siehe Kopfzeile von src/scene.h).
//
// Pruefungen:
//   1) Blattpuffer, Karten und Huellen eines Baums sind UEBER ALL Build-
//      Entfernungen gleich, in denen die Schicht laut Existenz-Schnitt existieren
//      darf. (Existenz-Schnitte duerfen eine Schicht ganz abschalten, aber nie
//      ihre Menge veraendern.)
//   2) Es gibt keinen Abstand innerhalb der Baukante, an dem ALLE Laub-Schichten
//      fehlen (nackter Stamm).
//   3) Existenz-Schnitt und Schichtgewicht fallen zusammen: ist eine Schicht im
//      Puffer, kann ihr Gewicht > 0 sein; fehlt sie, ist ihr Gewicht 0.
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene.h"
#include "tree_lod.h"

static int fails = 0, checks = 0;
static void ok(int cond, const char *fmt, ...){
  checks++;
  if(cond) return;
  va_list ap; va_start(ap, fmt);
  printf("  FEHLER: "); vfprintf(stdout, fmt, ap); printf("\n");
  va_end(ap); fails++;
}

// Slot eines Baums ueber seinen Lotpunkt finden (deterministischer Wald).
static const TreeSlot *findSlot(float ax, float az){
  for(int i = 0; i < treeSlotND; i++){
    const TreeSlot *s = &treeSlotsD[i];
    if(fabsf(s->ax - ax) < 0.01f && fabsf(s->az - az) < 0.01f) return s;
  }
  return 0;
}

int main(void){
  const float focal = 1.7f * 360.0f;      // Bildhoehe 360 Zeilen
  trees_set_focal_px(focal);
  treeBuildRange = 20;                    // 20 Zellen a 9 m = 180 m Baukante

  // Referenzbaum: der entfernteste Baum eines Build um den Ursprung.
  trees_build(0.0f, 0.0f);
  if(treeSlotND <= 0){ printf("kein Wald gebaut\n"); return 1; }
  float rx = 0, rz = 0, best = -1;
  for(int i = 0; i < treeSlotND; i++){
    const TreeSlot *s = &treeSlotsD[i];
    float d = sqrtf(s->ax * s->ax + s->az * s->az);
    if(d > best){ best = d; rx = s->ax; rz = s->az; }
  }
  const TreeSlot *ref = findSlot(rx, rz);
  if(!ref){ printf("Referenzbaum nicht gefunden\n"); return 1; }
  printf("Referenzbaum (%.2f, %.2f) Wuchshoehe %.2f m, Buildabstand %.1f m\n",
         rx, rz, ref->h, best);
  printf("Baukante %.0f m, focalPx %.0f\n", (float)treeBuildRange * 9.0f, focal);
  printf("\n");
  printf("  Abstand   R1     R2    Blaetter  Karten  Huellen  Buschel  Raster   pres\n");

  int cardSeen = -1, hullSeen = -1, sprigSeen = -1, hullSeen2 = -1;
  unsigned hashSeen = 0;
  int cardOn = 0, hullOn = 0;
  for(int k = 0; k <= 20; k++){
    float d = 20.0f + 12.0f * (float)k;           // 20 .. 260 m
    // Build-Zentrum so legen, dass der Referenzbaum genau d entfernt ist.
    float nx = rx, nz = rz;
    float l = sqrtf(nx * nx + nz * nz); if(l < 1e-3f){ nx = 1; nz = 0; l = 1; }
    nx /= l; nz /= l;
    trees_build(rx - nx * d, rz - nz * d);
    const TreeSlot *s = findSlot(rx, rz);
    if(!s){
      printf("  %6.0f m  -- ausserhalb des Puffers --\n", d);
      // Jenseits der Baukante + Nachlauf darf er fehlen; innerhalb nicht.
      ok(d > (float)treeBuildRange * 9.0f + LOD_BUILD_LAG,
         "Baum fehlt bei %.0f m, Baukante ist %.0f m + %.0f m Nachlauf",
         d, (float)treeBuildRange * 9.0f, LOD_BUILD_LAG);
      continue;
    }
    printf("  %6.0f  %5.1f  %5.1f   %7d  %6d  %7d   %6d  %3dx%-3d %.2f  %d\n",
           d, s->R1, s->R2, s->leafN, s->cardN, s->hullN, s->sprigN,
           s->hzNaz, s->hzNhy, s->hzCs, s->pres);
    if(sprigSeen < 0){ sprigSeen = s->sprigN; hashSeen = s->sprigHash; }
    if(s->hullN > 0 && hullSeen2 < 0) hullSeen2 = s->hullN;
    else {
      if(s->sprigN != sprigSeen)
        ok(0, "Buschelzahl aendert sich: %d bei %.0f m statt %d", s->sprigN, d, sprigSeen);
      if(s->sprigHash != hashSeen)
        ok(0, "Buschel-POSITIONEN aendern sich: %08x bei %.0f m statt %08x",
           s->sprigHash, d, hashSeen);
      if(s->hullN > 0 && s->hullN != hullSeen2)
        ok(0, "Huellenzellen %d bei %.0f m statt %d (Raster %dx%d, Zelle %.2f)",
           s->hullN, d, hullSeen2, s->hzNaz, s->hzNhy, s->hzCs);
    }
    // 1) Menge einer vorhandenen Schicht darf sich nicht aendern
    if(s->cardN > 0){
      if(cardSeen < 0) cardSeen = s->cardN;
      ok(s->cardN == cardSeen, "Kartenmenge aendert sich: %d bei %.0f m statt %d",
         s->cardN, d, cardSeen);
      cardOn = 1;
    }
    if(s->hullN > 0){
      if(hullSeen < 0) hullSeen = s->hullN;
      ok(s->hullN == hullSeen, "Huellenmenge aendert sich: %d bei %.0f m statt %d",
         s->hullN, d, hullSeen);
      hullOn = 1;
    }
    // 2) nie alle Laub-Schichten weg, solange der Baum im Puffer ist
    ok(s->leafN + s->cardN + s->hullN > 0,
       "nackter Stamm bei %.0f m (kein Blatt, keine Karte, keine Huelle)", d);
    // 3) Existenz-Schnitt = Gewicht: fehlt eine Schicht, muss ihr Gewicht 0 sein
    TreeLod L; L.H = s->h; L.R1 = s->R1; L.R2 = s->R2; L.pres = s->pres;
    L.focalPx = focal; L.fern = (s->pres & LOD_PRES_BB) ? 1 : 0;
    L.bb = L.fern;
    float wCard = treeLodWCard(&L, d), wBB = treeLodWBillboard(&L, d);
    if(s->cardN == 0)
      ok(wCard <= 0.001f, "Karten fehlen bei %.0f m, Gewicht waere %.3f", d, wCard);
    if(s->hullN == 0)
      ok(wBB <= 0.001f, "Huelle fehlt bei %.0f m, Gewicht waere %.3f", d, wBB);
    if(s->cardN > 0 && d > s->R2 * 1.30f)
      ok(wCard <= 0.001f, "Karten gebaut bei %.0f m, Gewicht %.3f", d, wCard);
  }
  printf("\n  Karten konstant %d, Huellen konstant %d\n", cardSeen, hullSeen);
  ok(cardSeen > 0, "gar keine Karten gebaut");
  ok(hullSeen > 0, "gar keine Huellen gebaut");
  printf("== %d Pruefungen, %d Fehler ==\n", checks, fails);
  return fails ? 1 : 0;
}

/* test_tree_lod.c — Wirtstest des kanonischen Baum-LODs (Strategie B).
 *
 * Laeuft OHNE GL, OHNE Engine, in Millisekunden. Es prueft die Invarianten,
 * deren Verletzung der Nutzer als Sprung oder Pulsieren sieht:
 *
 *   1 Komplementaritaet  — die Gewichte der Schichten eines Baums summieren
 *                          sich fuer JEDE Entfernung zu 1 (kein Deckungsloch,
 *                          keine doppelte Schicht)
 *   2 Stetigkeit         — kein Gewicht springt bei 0,5 m Kameraweg
 *   3 Existenz vor Blende— Geometrie ist im Puffer, bevor ein Band sie braucht
 *   4 Hysterese          — an einer Schaltgrenze oszilliert die Stufe nicht
 *   5 GLSL == C          — Shader und Kern benutzen dieselben Bandzahlen
 *   6 ein Anker pro Baum — alle drei Shader messen den LOD-Abstand am Lotpunkt
 *
 * Aufruf aus der Projektwurzel: make test-lod
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/tree_lod.h"

static int fails = 0, checks = 0;

static void ok(int cond, const char *fmt, ...){
  checks++;
  if(cond) return;
  fails++;
  va_list ap; va_start(ap, fmt);
  printf("  FEHLER: "); vprintf(fmt, ap); printf("\n");
  va_end(ap);
}

/* ---- Datei-Helfer fuer die Quelltext-Pruefungen ---- */
static char *slurp(const char *path, size_t *len){
  FILE *f = fopen(path, "rb");
  if(!f) return NULL;
  fseek(f, 0, SEEK_END); *len = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
  char *b = malloc(*len + 1);
  if(fread(b, 1, *len, f) != *len){ free(b); fclose(f); return NULL; }
  b[*len] = 0; fclose(f); return b;
}
static int has(const char *path, const char *needle){
  size_t n; char *b = slurp(path, &n);
  if(!b){ ok(0, "%s nicht lesbar", path); return 0; }
  int found = strstr(b, needle) != NULL;
  free(b);
  return found;
}
static int lacks(const char *path, const char *needle){
  size_t n; char *b = slurp(path, &n);
  if(!b){ ok(0, "%s nicht lesbar", path); return 0; }
  int found = strstr(b, needle) != NULL;
  free(b);
  return !found;
}

int main(void){
  printf("== Baum-LOD Wirtstest (ohne GL) ==\n");

  /* 1 + 2: Gewichte ueber die Entfernung, mehrere Wuchshoehen
   * (die Klemmen in treeLodInit erzeugen verschiedene R1/R2-Verhaeltnisse) */
  const float Hs[] = { 2.0f, 4.0f, 8.0f, 16.0f, 40.0f };
  // beide Betriebsarten: mit und ohne Ganzbaum-Kacheln im Puffer
  const int BBs[] = { 1, 0 };
  for(unsigned hi = 0; hi < sizeof(Hs)/sizeof(Hs[0]); hi++)
  for(unsigned bi = 0; bi < 2; bi++){
    TreeLod L; treeLodInit(&L, Hs[hi], 0.0f); L.bb = BBs[bi];
    float dmax = 1.6f * L.R2;
    float sumWorst = 0.0f, stepWorst = 0.0f;
    float pw[4] = {0,0,0,0};
    for(float d = 0.0f; d <= dmax; d += 0.1f){
      float w[4] = { treeLodWLeaf(&L, d), treeLodWCard(&L, d),
                     treeLodWBillboard(&L, d), treeLodWMesh(&L, d) };
      // FIZZLE-INVARIANTE: die Gewichte muessen sich exakt zu 1 ergaenzen. Dann
      // sind die Behalt-Mengen "n < w" (abgehend) und "n >= 1-w" (kommend) fuer
      // dasselbe Rauschen disjunkt und ihre Vereinigung ist die ganze Flaeche —
      // durchgehend 100 % Deckung, ohne Alpha-Blend, ohne Sortieren.
      float sum = w[0] + w[1] + w[2];
      if(fabsf(sum - 1.0f) > sumWorst) sumWorst = fabsf(sum - 1.0f);
      ok(fabsf(w[3] + w[2] - 1.0f) < 1e-4f,
         "H=%.0f d=%.1f: Geruest+Billboard = %.4f (erwartet 1)", Hs[hi], d, w[3]+w[2]);
      if(d > 0.0f){
        for(int k = 0; k < 4; k++){
          float s = fabsf(w[k] - pw[k]);
          if(s > stepWorst) stepWorst = s;
        }
      }
      for(int k = 0; k < 4; k++) pw[k] = w[k];
    }
    checks++;
    if(sumWorst > 1e-4f){
      fails++;
      printf("  FEHLER: H=%.0f: Laub-Deckung weicht ab, worst |Summe-1| = %.5f\n",
             Hs[hi], sumWorst);
    }
    /* Stetigkeit: die groesste Steigung eines smoothstep ueber ein Band der
     * Breite w ist 1,5/w. Ein 0,5-m-Schritt darf diese Grenze nicht ueberschreiten
     * — alles darueber waere ein echter Sprung. Die Bandbreite selbst ist die
     * Designgrenze: bei 4 m Band und 5 m/s Kamerageschwindigkeit laeuft die
     * Blende ueber 0,8 s, das ist die untere Grenze, die noch weich aussieht. */
    float band1 = L.R1 * (LOD_BAND_HI - LOD_BAND_LO);
    float band2 = L.R2 * (LOD_BAND_HI - LOD_BAND_LO);
    ok(band1 >= 4.0f && band2 >= 4.0f,
       "H=%.0f: Blendenband zu schmal (R1-Band %.1f m, R2-Band %.1f m)",
       Hs[hi], band1, band2);
    ok(sumWorst <= 1e-4f, "H=%.0f bb=%d: Gewichte ergaenzen sich nicht zu 1 (worst %.4f)",
       Hs[hi], L.bb, sumWorst);
    float lim = 1.5f * 0.5f / (band1 < band2 ? band1 : band2) * 1.02f;
    ok(stepWorst * 5.0f <= lim,
       "H=%.0f: Gewichte springen — max Delta bei 0,5 m = %.3f, Grenze %.3f",
       Hs[hi], stepWorst*5.0f, lim);
    printf("  H=%2.0f m  bb=%d  R1=%5.1f R2=%6.1f  |Summe-1|max=%.1e  Sprung/0,5m=%.3f\n",
           Hs[hi], L.bb, L.R1, L.R2, sumWorst, stepWorst*5.0f);
  }

  /* 3: Existenz-Raender muessen ausserhalb des aeussersten Bandendes liegen */
  for(unsigned hi = 0; hi < sizeof(Hs)/sizeof(Hs[0]); hi++){
    TreeLod L; treeLodInit(&L, Hs[hi], 0.0f);
    ok(treeLodLeafCut(&L)   >= L.R1 * LOD_BAND_HI,
       "H=%.0f: Blatt-Existenz (%.1f) endet VOR dem Blattband (%.1f)",
       Hs[hi], treeLodLeafCut(&L), L.R1 * LOD_BAND_HI);
    ok(treeLodBranchCut(&L) >= L.R2 * LOD_BAND_HI,
       "H=%.0f: Ast-Existenz (%.1f) endet VOR dem Billboard-Band (%.1f)",
       Hs[hi], treeLodBranchCut(&L), L.R2 * LOD_BAND_HI);
    ok(treeLodMeshCut(&L)   >= L.R2 * LOD_BAND_HI,
       "H=%.0f: Netz-Existenz (%.1f) endet VOR dem Billboard-Band (%.1f)",
       Hs[hi], treeLodMeshCut(&L), L.R2 * LOD_BAND_HI);
  }
  { TreeLod L; treeLodInit(&L, 8.0f, 0.0f);
    printf("  Existenz-Raender (H=8): Blatt %.1f m >= %.1f m, Ast %.1f m >= %.1f m\n",
           treeLodLeafCut(&L), L.R1 * LOD_BAND_HI, treeLodBranchCut(&L), L.R2 * LOD_BAND_HI); }

  /* 3b: ein Schnitt darf nie in einem Bereich mit Gewicht > 0 liegen.
     * Getestet wird ueber die ganze Bau-Reichweite (180 m): wherever
     * weight(d) > 0, die Geometrie muss existieren (d <= cut). */
  for(unsigned hi = 0; hi < sizeof(Hs)/sizeof(Hs[0]); hi++)
  for(unsigned bi = 0; bi < 2; bi++){
    TreeLod L; treeLodInit(&L, Hs[hi], 0.0f); L.bb = BBs[bi];
    for(float d = 0.0f; d <= 180.0f; d += 0.5f){
      ok(!(treeLodWMesh(&L, d) > 0.0f) || d <= treeLodBranchCut(&L),
         "H=%.0f bb=%d: Geruest-Gewicht %.2f bei %.1f m, aber Ast-Schnitt bei %.1f m",
         Hs[hi], L.bb, treeLodWMesh(&L, d), d, treeLodBranchCut(&L));
      ok(!(treeLodWLeaf(&L, d) > 0.0f) || d <= treeLodLeafCut(&L),
         "H=%.0f bb=%d: Blatt-Gewicht %.2f bei %.1f m, aber Blatt-Schnitt bei %.1f m",
         Hs[hi], L.bb, treeLodWLeaf(&L, d), d, treeLodLeafCut(&L));
    }
  }

  /* 3c: Bildgroessen-LOD des Holzes. Gewicht faellt mit der Entfernung, und der
     * Existenz-Schnitt liegt genau dort, wo es 0 wird (doppelte Randzunaehme:
     * direkt jenseits des Schnitts muss 0 stehen, direkt innerhalb darf es > 0). */
  for(unsigned hi = 0; hi < sizeof(Hs)/sizeof(Hs[0]); hi++){
    TreeLod L; treeLodInit(&L, Hs[hi], 0.0f); L.focalPx = 1836.0f;   // 1080p
    float rads[4] = { 0.004f, 0.01f, 0.05f, 0.15f };
    for(int ri = 0; ri < 4; ri++){
      float cut = treeLodRadCut(&L, rads[ri]);
      ok(cut > 0.0f && cut < 1e8f, "RadCut positiv und endlich (rad %.3f)", rads[ri]);
      ok(treeLodWBranch(&L, rads[ri], cut * 1.05f) == 0.0f,
         "jenseits des Schnitts Gewicht 0 (rad %.3f, cut %.1f)", rads[ri], cut);
      ok(treeLodWBranch(&L, rads[ri], cut * 0.95f) > 0.0f,
         "innerhalb des Schnitts Gewicht > 0 (rad %.3f, cut %.1f)", rads[ri], cut);
      float prev = 2.0f;   // Startwert ueber dem maximum, sonst ist der erste Vergleich eine Stufe
      for(float d = 1.0f; d <= 400.0f; d += 1.0f){
        float w = treeLodWBranch(&L, rads[ri], d);
        ok(w <= prev + 1e-6f, "Ast-Gewicht nicht monoton (rad %.3f bei %.0f m)", rads[ri], d);
        prev = w;
      }
    }
    /* Brennweite 0 = nicht gesetzt -> kein Schnitt (nichts darf verschwinden). */
    TreeLod Z; treeLodInit(&Z, Hs[hi], 0.0f); Z.focalPx = 0.0f;
    ok(treeLodRadCut(&Z, 0.01f) >= 1e8f, "ohne Brennweite kein Schnitt");
  }

  /* 4: Hysterese — Kamera wackelt an einer Schaltgrenze */
  for(unsigned hi = 0; hi < sizeof(Hs)/sizeof(Hs[0]); hi++){
    TreeLod L; treeLodInit(&L, Hs[hi], 0.0f);
    const float bounds[2] = { L.R1, L.R2 };
    for(int b = 0; b < 2; b++){
      int prev = -1, changes = 0;
      for(int i = 0; i < 200; i++){
        float d = bounds[b] + ((i % 2) ? 0.5f : -0.5f);   // +/- 0,5 m Wackeln
        int t = treeLodTier(&L, d, prev);
        if(prev >= 0 && t != prev) changes++;
        prev = t;
      }
      ok(changes <= 1, "H=%.0f Grenze R%d: Stufe wechselt %d mal bei +/-0,5 m Wackeln",
         Hs[hi], b+1, changes);
    }
    /* monoton herausgehen: Stufe darf nie wieder hochspringen */
    int prev = -1, prevTier = -1, ups = 0;
    for(float d = 0.0f; d <= 1.5f * L.R2; d += 0.5f){
      int t = treeLodTier(&L, d, prev);
      if(prevTier >= 0 && t > prevTier) ups++;
      prev = t; prevTier = t;
    }
    ok(ups == 0, "H=%.0f: Stufe steigt beim Entfernen %d mal wieder an", Hs[hi], ups);
  }

  /* 5: GLSL-Seite == C-Kern */
  char lo[32], hi2[32];
  snprintf(lo, sizeof lo, "R * %.2f", (double)LOD_BAND_LO);
  snprintf(hi2, sizeof hi2, "R * %.2f", (double)LOD_BAND_HI);
  ok(has("src/shaders/lod.glsl", lo),  "lod.glsl enthaelt nicht %s", lo);
  ok(has("src/shaders/lod.glsl", hi2), "lod.glsl enthaelt nicht %s", hi2);
  const char *shaders[] = { "src/shaders/leaf.vert", "src/shaders/crown.vert",
                            "src/shaders/tree.vert" };
  for(int s = 0; s < 3; s++){
    ok(has(shaders[s], "#include \"lod.glsl\""), "%s bindet lod.glsl nicht ein", shaders[s]);
    /* keine handgeschriebene Bande mehr in den Shadern */
    ok(lacks(shaders[s], "* 0.90") && lacks(shaders[s], "* 1.10"),
       "%s enthaelt eine handgeschriebene Bande", shaders[s]);
    /* 6: LOD-Abstand am Lotpunkt des Baums, nicht an Blatt/Kartenmitte/Vertex */
    int anchor = has(shaders[s], "aAnchor") || has(shaders[s], "iAnchor");
    ok(anchor, "%s misst den LOD-Abstand ohne Baum-Anker", shaders[s]);
  }

  /* ---- BILDGROESSEN-SCHALTWEITEN (treeLodInitPx, Nutzer: "eine Funktion der
   * Blattgroesse und/oder der Flaeche auf dem Bildschirm") ---- */
  {
    struct { float leafM, cardM, focal; } C[] = {
      {0.05f,0.10f,612.0f},{0.09f,0.18f,612.0f},{0.14f,0.28f,612.0f},
      {0.05f,0.10f,1836.0f},{0.09f,0.18f,1836.0f},{0.14f,0.28f,1836.0f},
      {0.001f,0.001f,1836.0f},{0.09f,0.18f,0.0f} };
    for(int ci = 0; ci < (int)(sizeof(C)/sizeof(C[0])); ci++){
      TreeLod L; treeLodInitPx(&L, C[ci].leafM, C[ci].cardM, C[ci].focal, 0.0f);
      float w1 = C[ci].leafM * C[ci].focal / LOD_LEAF_PX;
      if(C[ci].focal <= 0.0f) w1 = 30.0f;
      if(w1 < 8.0f) w1 = 8.0f; else if(w1 > 160.0f) w1 = 160.0f;
      ok(fabsf(L.R1 - w1) < 1e-3f, "R1 = %.2f, erwartet %.2f (Blatt %.3f m, focal %.0f px)",
         L.R1, w1, C[ci].leafM, C[ci].focal);
      ok(L.R2 >= 1.5f * L.R1 - 1e-3f, "R2 = %.2f liegt unter dem 1,5-fachen von R1 = %.2f", L.R2, L.R1);
      ok(L.R2 <= 320.0f + 1e-3f, "R2 = %.2f ueber 320 m", L.R2);
      for(int bi = 0; bi < 2; bi++){
        L.bb = bi;
        float worst = 0.0f, slope = 0.0f, prev = -1.0f;
        for(float d = 0.0f; d <= 1.5f * L.R2; d += 0.5f){
          float w[4] = { treeLodWLeaf(&L, d), treeLodWCard(&L, d),
                         treeLodWBillboard(&L, d), treeLodWMesh(&L, d) };
          float sum = w[0] + w[1] + w[2];
          if(fabsf(sum - 1.0f) > worst) worst = fabsf(sum - 1.0f);
          if(prev >= 0.0f){ float s2 = fabsf(w[0] - prev); if(s2 > slope) slope = s2; }
          prev = w[0];
        }
        ok(worst < 1e-4f, "Blatt %.3f m focal %.0f bb=%d: Gewichte weichen von 1 ab (worst %.1e)",
           C[ci].leafM, C[ci].focal, bi, worst);
        float lim = 1.5f * 0.5f / (0.2f * L.R1) * 1.05f;
        ok(slope <= lim, "Blatt %.3f m: Blattgewicht springt %.3f pro 0,5 m (Schranke %.3f)",
           C[ci].leafM, slope, lim);
      }
    }
  }

  printf("== %d Pruefungen, %d Fehler ==\n", checks, fails);
  return fails ? 1 : 0;
}

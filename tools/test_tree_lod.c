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
      float sum = w[0] + w[1] + w[2];
      if(fabsf(sum - 1.0f) > sumWorst) sumWorst = fabsf(sum - 1.0f);
      /* Geruest und Ganzbaum duerfen sich nie ueberlappen: Summe = 1 */
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
    float lim = 1.5f * 0.5f / (band1 < band2 ? band1 : band2) * 1.02f;
    ok(stepWorst * 5.0f <= lim,
       "H=%.0f: Gewichte springen — max Delta bei 0,5 m = %.3f, analytische Grenze %.3f",
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

  printf("== %d Pruefungen, %d Fehler ==\n", checks, fails);
  return fails ? 1 : 0;
}

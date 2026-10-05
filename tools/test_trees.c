// test_trees.c - Unit-Test fuer trees_cpu/grass_cpu/terrain_cpu (ohne GL).
// Bau: gcc -O2 -Wall -Wextra -Isrc -o /tmp/test_trees tools/test_trees.c \
//              src/trees_cpu.c src/grass_cpu.c src/terrain_cpu.c -lm
#include <stdio.h>
#include <math.h>
#include "scene.h"

static int check_range(const float *v, int n, int floats, const char *name,
                       int *nan, float *lo, float *hi){
  for(int i = 0; i < n * floats; i++){
    float x = v[i];
    if(isnanf(x) || isinff(x)){ (*nan)++; continue; }
    if(x < *lo) *lo = x;
    if(x > *hi) *hi = x;
  }
  printf("%-12s %7d Eintraege  range [%.2f .. %.2f]  %d NaN/Inf\n",
         name, n, *lo, *hi, *nan);
  return *nan == 0;
}

int main(void){
  printf("== trees_build(0,0) ==\n");
  trees_build(0.0f, 0.0f);
  printf("Baeume: %d  Mesh-Vertices: %d  Blaetter: %d  Crown-Vertices: %d  Wire-Vertices: %d\n",
         nTrees, nTreeVerts, nLeaves, nCrownVerts, nWireVerts);
  int nan; float lo, hi;
  int ok = 1;
  nan = 0; lo = 1e9f; hi = -1e9f;
  ok &= check_range(treeVerts, nTreeVerts, TREE_FLOATS, "treeVerts", &nan, &lo, &hi);
  nan = 0; lo = 1e9f; hi = -1e9f;
  ok &= check_range(leafInst, nLeaves, LEAF_FLOATS, "leafInst", &nan, &lo, &hi);
  nan = 0; lo = 1e9f; hi = -1e9f;
  ok &= check_range(crownVerts, nCrownVerts, CROWN_FLOATS, "crownVerts", &nan, &lo, &hi);
  nan = 0; lo = 1e9f; hi = -1e9f;
  ok &= check_range(wireVerts, nWireVerts, WIRE_FLOATS, "wireVerts", &nan, &lo, &hi);

  // Blaetter muessen ueber dem Terrain sitzen (mit Toleranz fuer Blattgroesse)
  int above = 0, below = 0;
  for(int i = 0; i < nLeaves; i++){
    float x = leafInst[i * LEAF_FLOATS + 0];
    float y = leafInst[i * LEAF_FLOATS + 1];
    float h = c_terrainH(x, leafInst[i * LEAF_FLOATS + 2]);
    if(y > h - 0.05f) above++; else below++;
  }
  printf("Blaetter ueber Terrain: %d / unter Terrain: %d\n", above, below);

  // Verteilung: Baeume sollten flaechig sein (kein Linien-Artefakt)
  // Korrelation x/z der Blatt-Positionen (Muster aus dem alten Bug: ~0.95)
  {
    double sx = 0, sz = 0, sxx = 0, szz = 0, sxz = 0;
    int n = nLeaves;
    for(int i = 0; i < n; i++){
      double x = leafInst[i * LEAF_FLOATS + 0];
      double z = leafInst[i * LEAF_FLOATS + 2];
      sx += x; sz += z; sxx += x * x; szz += z * z; sxz += x * z;
    }
    double cov = sxz / n - sx / n * sz / n;
    double vx = sxx / n - sx / n * sx / n;
    double vz = szz / n - sz / n * sz / n;
    double corr = cov / sqrt(vx * vz);
    printf("Korrelation x/z der Blaetter: %.3f (soll ~0 sein)\n", corr);
    if(fabs(corr) > 0.3) ok = 0;
  }

  printf("\n== grass_build(30,34) (Kamera-Startposition) ==\n");
  grass_build(30.0f, 34.0f);
  printf("Halme: %d\n", nGrass);
  nan = 0; lo = 1e9f; hi = -1e9f;
  ok &= check_range(grassInst, nGrass, GRASS_FLOATS, "grassInst", &nan, &lo, &hi);
  (void)0;
  int overWater = 0;
  for(int i = 0; i < nGrass; i++){
    float x = grassInst[i * GRASS_FLOATS + 0];
    float z = grassInst[i * GRASS_FLOATS + 2];
    if(c_terrainH(x, z) < WATER_Y + 0.05f) overWater++;
  }
  printf("Halme im Wasser: %d (soll 0 sein)\n", overWater);
  if(overWater > 0) ok = 0;

  printf("\n== Rebuild-Stabilitaet ==\n");
  trees_build(0.0f, 0.0f);
  int t1 = nTrees;
  trees_build(0.5f, 0.5f);   // Patch rutscht leicht -> gleiche Baeume im Ueberlapp
  printf("Baeume nach leichtem Versatz: %d (vorher %d)\n", nTrees, t1);
  grass_build(30.0f, 34.0f);
  int g1 = nGrass;
  grass_build(30.4f, 34.0f);
  printf("Halme nach 0.4m Versatz: %d (vorher %d, Abweichung %+.1f%%)\n",
         nGrass, g1, 100.0 * (nGrass - g1) / g1);

  printf("\n%s\n", ok ? "ALLE TESTS OK" : "TESTFEHLER");
  return ok ? 0 : 1;
}
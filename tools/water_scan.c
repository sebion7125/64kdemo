// Water-Scan: Terrain-Hehoehe um den Ursprung analysieren, Kandidaten fuer
// Wasserstand bestimmen. 1:1-Port der Hash/Noise/Terrain-Funktionen aus main.c.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static float cfract(float x){ return x - floorf(x); }
static float c_hash12(float px, float py){
  float p3x = cfract(px * 0.1031f);
  float p3y = cfract(py * 0.1031f);
  float p3z = cfract(px * 0.1031f);
  float d = p3x * (p3y + 33.33f) + p3y * (p3z + 33.33f) + p3z * (p3x + 33.33f);
  p3x += d; p3y += d; p3z += d;
  return cfract((p3x + p3y) * p3z);
}
static float c_vnoise(float px, float py){
  float ix = floorf(px), iy = floorf(py);
  float fx = px - ix, fy = py - iy;
  float ux = fx * fx * (3.0f - 2.0f * fx);
  float uy = fy * fy * (3.0f - 2.0f * fy);
  float a = c_hash12(ix, iy);
  float b = c_hash12(ix + 1.0f, iy);
  float c = c_hash12(ix, iy + 1.0f);
  float d = c_hash12(ix + 1.0f, iy + 1.0f);
  float ab = a + (b - a) * ux;
  float cd = c + (d - c) * ux;
  return ab + (cd - ab) * uy;
}
static float c_fbm(float px, float py){
  float s = 0.0f, a = 0.5f;
  for(int i = 0; i < 4; i++){
    s += a * c_vnoise(px, py);
    px = px * 2.03f + 17.1f;
    py = py * 2.03f + 9.2f;
    a *= 0.5f;
  }
  return s;
}
static float c_rfbm(float px, float py){
  float s = 0.0f, a = 0.5f;
  for(int i = 0; i < 4; i++){
    s += a * (1.0f - fabsf(2.0f * c_vnoise(px, py) - 1.0f));
    px = px * 2.11f + 9.7f;
    py = py * 2.11f + 3.1f;
    a *= 0.5f;
  }
  return s;
}
static float c_terrainH(float px, float py){
  float base = c_fbm(px * 0.010f, py * 0.010f);
  float t = (base - 0.38f) / (0.72f - 0.38f);
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  float mask = t * t * (3.0f - 2.0f * t);
  float r = c_rfbm(px * 0.017f, py * 0.017f);
  float mts = powf(r, 2.5f) * 32.0f * mask;
  float hills = (c_fbm(px * 0.045f, py * 0.045f) - 0.5f) * 5.0f;
  float det = (c_fbm(px * 0.40f, py * 0.40f) - 0.5f) * 0.3f;
  return hills + mts + det;
}

static inline float length2(float x, float z){ return sqrtf(x * x + z * z); }

int main(void){
  const int R = 160;      // Scan-Radius in m
  const int S = 160;      // Grid-Auflösung
  const float step = (2.0f * R) / S;
  // 1) Hoehenstatistik
  float hmin = 1e9, hmax = -1e9;
  float *H = malloc((size_t)S * S * sizeof(float));
  for(int j = 0; j < S; j++)
  for(int i = 0; i < S; i++){
    float x = -R + (i + 0.5f) * step;
    float z = -R + (j + 0.5f) * step;
    float h = c_terrainH(x, z);
    H[j * S + i] = h;
    if(h < hmin) hmin = h;
    if(h > hmax) hmax = h;
  }
  printf("Terrain um Ursprung (±%dm): min=%.2f max=%.2f\n", R, hmin, hmax);
  // 2) Hoehenhistogramm (nur nahe am Ursprung, wo die Kamera hinschaut)
  for(int lvl = (int)hmin; lvl <= (int)hmax + 1; lvl++){
    int cnt = 0, cntNear = 0;
    for(int j = 0; j < S; j++)
    for(int i = 0; i < S; i++){
      float x = -R + (i + 0.5f) * step, z = -R + (j + 0.5f) * step;
      if(H[j * S + i] < lvl + 0.5f) cnt++;
      if(length2(x, z) < 60.0f * 60.0f && H[j * S + i] < lvl + 0.5f) cntNear++;
    }
    printf("  < %4d: %5.1f%% gesamt, %5.1f%% inner 60m\n", lvl + 1,
           100.0 * cnt / (S * S), 100.0 * cntNear / (S * S));
  }
  // 3) ASCII-Heightmap (32x32, 0..9 = hoehe)
  printf("\nHeightmap (10x10m-Blöcke, 0-9 -> U-t): +x rechts, +z unten\n");
  const int CS = 32;
  for(int j = 0; j < CS; j++){
    for(int i = 0; i < CS; i++){
      float x = -R + (i + 0.5f) * (2.0f * R / CS);
      float z = -R + (j + 0.5f) * (2.0f * R / CS);
      float h = c_terrainH(x, z);
      int v = (int)((h - hmin) / (hmax - hmin) * 9.999f);
      if(v < 0) v = 0; if(v > 9) v = 9;
      printf("%c", "0123456789"[v]);
    }
    printf("\n");
  }
  return 0;
}
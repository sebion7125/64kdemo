// Water-Scan 2: fuer Kandidaten-Wasserstaende -> Flaechen, Bounding Box der
// groessten zusammenhaengenden Wasserregion, Sichtbarkeit aus der Kamera.
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

int main(void){
  const int R = 200, S = 200;
  const float step = (2.0f * R) / S;
  static float H[200 * 200];
  static int visited[200 * 200];
  for(int j = 0; j < S; j++)
  for(int i = 0; i < S; i++)
    H[j * S + i] = c_terrainH(-R + (i + 0.5f) * step, -R + (j + 0.5f) * step);

  float Ws[] = { 0.10f, 0.20f, 0.25f, 0.30f, 0.40f, 0.50f };
  for (int w = 0; w < 6; w++){
    float W = Ws[w];
    int total = 0;
    for(int j = 0; j < S; j++)
    for(int i = 0; i < S; i++)
      if(H[j * S + i] < W) total++;
    // Flood Fill: groesste zusammenhaengende Region
    for(int i = 0; i < S * S; i++) visited[i] = 0;
    int best = 0; float bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    int qx[40000], qy[40000];
    for(int j = 0; j < S; j++)
    for(int i = 0; i < S; i++){
      if(H[j * S + i] >= W || visited[j * S + i]) continue;
      int head = 0, tail = 0;
      qx[tail] = i; qy[tail] = j; tail++;
      visited[j * S + i] = 1;
      float rx0 = 1e9, ry0 = 1e9, rx1 = -1e9, ry1 = -1e9;
      int cnt = 0;
      while(head < tail){
        int ci = qx[head], cj = qy[head]; head++;
        cnt++;
        float x = -R + (ci + 0.5f) * step, z = -R + (cj + 0.5f) * step;
        if(x < rx0) rx0 = x; if(x > rx1) rx1 = x;
        if(z < ry0) ry0 = z; if(z > ry1) ry1 = z;
        int nb[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for(int k = 0; k < 4; k++){
          int ni = ci + nb[k][0], nj = cj + nb[k][1];
          if(ni < 0 || nj < 0 || ni >= S || nj >= S) continue;
          if(H[nj * S + ni] < W && !visited[nj * S + ni]){
            visited[nj * S + ni] = 1;
            qx[tail] = ni; qy[tail] = nj; tail++;
          }
        }
      }
      if(cnt > best){
        best = cnt; bx0 = rx0; by0 = ry0; bx1 = rx1; by1 = ry1;
      }
    }
    printf("W=%.2f: Wasser=%5.1f%%  groesste Region=%5.1f%%  Box x[%.0f..%.0f] z[%.0f..%.0f]\n",
           W, 100.0 * total / (S * S), 100.0 * best / (S * S), bx0, bx1, by0, by1);
  }
  return 0;
}
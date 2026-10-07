// terrain_cpu.c - CPU-Port der Hash/Noise/Terrain-Funktionen aus fragment.glsl.
// 1:1-Port: gleiche Konstanten, gleiche Reihenfolge -> identische Werte wie der
// Shader (notwendig, damit Mesh-Platzierung und Raymarcher-Terrain uebereinstimmen).
#include "scene.h"
#include <math.h>

static float cfract(float x){ return x - floorf(x); }

float c_hash12(float px, float py){
  float p3x = cfract(px * 0.1031f);
  float p3y = cfract(py * 0.1031f);
  float p3z = cfract(px * 0.1031f);
  float d = p3x * (p3y + 33.33f) + p3y * (p3z + 33.33f) + p3z * (p3x + 33.33f);
  p3x += d; p3y += d; p3z += d;
  return cfract((p3x + p3y) * p3z);
}
void c_hash22(float px, float py, float *rx, float *ry){
  float p3x = cfract(px * 0.1031f);
  float p3y = cfract(py * 0.1030f);
  float p3z = cfract(px * 0.0973f);
  float d = p3x * (p3y + 33.33f) + p3y * (p3z + 33.33f) + p3z * (p3x + 33.33f);
  p3x += d; p3y += d; p3z += d;
  *rx = cfract((p3x + p3y) * p3z);
  *ry = cfract((p3x + p3z) * p3y);
}
float c_vnoise(float px, float py){
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
float c_fbm(float px, float py){
  float s = 0.0f, a = 0.5f;
  for(int i = 0; i < 4; i++){
    s += a * c_vnoise(px, py);
    px = px * 2.03f + 17.1f;
    py = py * 2.03f + 9.2f;
    a *= 0.5f;
  }
  return s;
}
float c_rfbm(float px, float py){
  float s = 0.0f, a = 0.5f;
  for(int i = 0; i < 4; i++){
    s += a * (1.0f - fabsf(2.0f * c_vnoise(px, py) - 1.0f));
    px = px * 2.11f + 9.7f;
    py = py * 2.11f + 3.1f;
    a *= 0.5f;
  }
  return s;
}
float c_terrainH(float px, float py){
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
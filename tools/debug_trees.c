// Debug: berechnet die Baum-Positionen (gleiche Logik wie fragment.glsl)
// und schreibt sie in eine Datei, um das Muster zu analysieren.
#include <stdio.h>
#include <math.h>

static double fractf(double x){ return x - floor(x); }

// exakt wie im Shader:
// vec2 hash22(vec2 p){
//   vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
//   p3 += dot(p3, p3.yzx + 33.33);
//   return fract((p3.xx + p3.yz) * p3.zy);
// }
static void hash22(double px, double py, double *rx, double *ry){
  double p3x = fractf(px * 0.1031);
  double p3y = fractf(py * 0.1030);
  double p3z = fractf(px * 0.0973);
  double d = p3x * (p3y + 33.33) + p3y * (p3z + 33.33) + p3z * (p3x + 33.33);
  p3x += d; p3y += d; p3z += d;
  *rx = fractf((p3x + p3y) * p3z);
  *ry = fractf((p3x + p3z) * p3y);
}

int main(){
  const double G = 9.0;
  FILE *f = fopen("/tmp/trees.txt", "w");
  if(!f) return 1;
  int count = 0;
  for(int iy = -15; iy <= 15; iy++)
  for(int ix = -15; ix <= 15; ix++){
    double cx = (double)ix, cy = (double)iy;
    double hx = cx * 0.37 + 7.31;
    double hy = cy * 0.37 + 3.17;
    double rx, ry;
    hash22(hx, hy, &rx, &ry);
    if(rx < 0.30) continue;
    double tp_x = (cx + 0.5 + (rx - 0.5) * 0.9) * G;
    double tp_z = (cy + 0.5 + (ry - 0.5) * 0.9) * G;
    fprintf(f, "%.3f %.3f  (cell %d,%d  rnd %.3f,%.3f)\n", tp_x, tp_z, ix, iy, rx, ry);
    count++;
  }
  fclose(f);
  printf("%d Baeume nach /tmp/trees.txt\n", count);
  return 0;
}
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aCol;   // rgb + sway-Faktor
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uTime;
out vec3 vNrm;
out vec3 vPos;
out vec4 vCol;
float hash12(vec2 p){
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
void main(){
  vec3 p = aPos;
  // Wind: Phase pro Baum (grobe Zelle ~15m, ein Baum bleibt in einer Zelle)
  float ph = hash12(floor(aPos.xz / 15.0)) * 6.283;
  float sw = aCol.w;
  p.x += sin(uTime * 1.4 + ph) * 0.05 * sw;
  p.z += cos(uTime * 1.1 + ph * 1.3) * 0.04 * sw;
  vNrm = aNrm;
  vPos = p;
  vCol = aCol;
  gl_Position = uMVP * vec4(p, 1.0);
}
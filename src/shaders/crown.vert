#version 330 core
layout(location=0) in vec3 aCenter;  // Quad-Mitte (Welt)
layout(location=1) in vec2 aOff;     // Ecke ±0.5
layout(location=2) in vec4 aData;    // size, rotY, seed, pad
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform vec3 uCamTgt;
out vec2 vUV;
out vec3 vPos;
out float vSeed;
out float vFade;
void main(){
  // Kamera-Achsen exakt wie der Raymarcher
  vec3 fwd = normalize(uCamTgt - uCamPos);
  vec3 right = normalize(cross(fwd, vec3(0,1,0)));
  vec3 up = cross(right, fwd);
  // Quad in der Kameraebene, um rotY (aData.y) gedreht
  float cr = cos(aData.y), sr = sin(aData.y);
  vec2 ro = vec2(aOff.x * cr - aOff.y * sr, aOff.x * sr + aOff.y * cr);
  vec3 p = aCenter + right * (ro.x * aData.x) + up * (ro.y * aData.x);
  float d = length(aCenter - uCamPos);
  // Blattdach ist in allen Distanzen sichtbar; nur am Horizont ausblenden
  vFade = 1.0 - smoothstep(120.0, 160.0, d);
  vUV = aOff * 2.0;
  vPos = p;
  vSeed = aData.z;
  gl_Position = uMVP * vec4(p, 1.0);
}
#version 330 core
layout(location=0) in vec2 aBase;   // (t laengs, w quer) Grundblatt
layout(location=1) in vec3 iPos;    // Blattbasis (Welt)
layout(location=2) in vec4 iData;   // size, rotY, tilt, lod-Pad
layout(location=3) in vec3 iCol;
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uTime;
out vec3 vNrm;
out vec3 vPos;
out vec3 vCol;
out float vT;
float hash12(vec2 p){
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
void main(){
  float size = iData.x, rotY = iData.y, tilt = iData.z;
  float t = aBase.x, w = aBase.y;
  float d = length(iPos - uCamPos);
  // LOD: Blaeue schrumpft in der Distanz (Kronen-Billboard uebernimmt)
  float lod = 1.0 - smoothstep(36.0, 50.0, d);
  if(lod < 0.004){ gl_Position = vec4(2.0, 0.0, 0.0, 1.0); return; }
  // lokales Blatt: Laenge +X, Breite +Z, Normal +Y
  vec3 lp = vec3(t * size * lod, 0.0, w * size * lod);
  // Ueberhang (Rotation um Z): Spitze haeengt
  float ct = cos(tilt), st = sin(tilt);
  lp = vec3(lp.x * ct, -lp.x * st, lp.z);
  // leiser Biegung + Wind
  lp.y += t * t * size * 0.22;
  float ph = hash12(floor(iPos.xz / 15.0)) * 6.283;
  lp.x += sin(uTime * 2.1 + ph + t * 2.5) * 0.05 * t;
  lp.y += cos(uTime * 1.7 + ph * 1.3 + t * 2.0) * 0.04 * t;
  // Drehung um Y
  float cr = cos(rotY), sr = sin(rotY);
  lp = vec3(lp.x * cr + lp.z * sr, lp.y, -lp.x * sr + lp.z * cr);
  vec3 p = iPos + lp;
  vec3 n = vec3(st * cr, ct, -st * sr);
  vNrm = n;
  vPos = p;
  vCol = iCol;
  vT = t;
  gl_Position = uMVP * vec4(p, 1.0);
}
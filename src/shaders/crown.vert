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
out vec2 vTile;      // Atlas-Kachel (Spalte, Zeile)
out vec3 vNrm;      // Billboard-Normale (zeigt zur Kamera)
out vec3 vRight, vUp;
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
  vFade = 1.0 - smoothstep(150.0, 185.0, d);
  vUV = aOff * 2.0;
  vPos = p;
  vSeed = aData.z;
  // Kachel nach Blickrichtung: aData.w = Kronen-Variante (0..3), azIdx = eine von
  // 16 Aufnahmerichtungen. Die Krone parallaxiert also, wenn man sie umlaeuft.
  float az = atan(aCenter.x - uCamPos.x, aCenter.z - uCamPos.z);
  float azIdx = floor(mod(az * (16.0 / 6.283185) + 8.0, 16.0));
  float t = aData.w * 16.0 + azIdx;
  vTile = vec2(mod(t, 8.0), floor(t / 8.0));
  vNrm = normalize(uCamPos - aCenter);
  vRight = right; vUp = up;
  gl_Position = uMVP * vec4(p, 1.0);
}
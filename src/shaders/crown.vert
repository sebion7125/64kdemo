#version 330 core
layout(location=0) in vec3 aCenter;  // Quad-Mitte (Welt)
layout(location=1) in vec2 aOff;     // Ecke ±0.5
layout(location=2) in vec4 aData;    // size, rotY, seed, pad
layout(location=3) in float iLodR;   // Radius, ab dem diese Karte das Laub dieses
                                     // Baumes eretzt (CPU: 10x Wuchshoehe)
layout(location=4) in vec3 iCol;     // Blattfarbe DIESES Baum
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform vec3 uCamTgt;
uniform float uTime;   // Wind (gleiches Boen-Feld wie Gras/Aeste/Blaetter)
uniform float uLightFace; // 1 = Quad steht senkrecht auf der Blickrichtung des
                          // Wurf-Betrachters (Sonne) — nur im Schatten-Pass.
uniform float uTreeFade = 1.0; // Ueberblendung beim LOD-Wechsel (Runde 27):
                               // 0 = dieser Satz verschwindet, 1 = voll sichtbar
out vec2 vUV;
out vec3 vPos;
out float vSeed;
out float vFade;
out vec2 vTile;      // Atlas-Kachel (Spalte, Zeile)
out vec3 vNrm;      // Billboard-Normale (zeigt zur Kamera)
out vec3 vRight, vUp;
out vec3 vCol;      // Blattfarbe des Baumes (Atlas ist farbneutral)
float hash12(vec2 p){
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float vnoise(vec2 p){
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i),           hash12(i + vec2(1,0)), u.x),
             mix(hash12(i + vec2(0,1)), hash12(i + vec2(1,1)), u.x), u.y);
}
void main(){
  // Kamera-Achsen exakt wie der Raymarcher
  vec3 fwd;
  if(uLightFace > 0.5){
    // Schatten-Wurf: uCamPos ist das AUGE DER SONNE. Ein kamera-zugewandtes
    // Billboard steht bei einer Sonne mit 24,6 Grad Erhebung fast kantig und
    // wirft nur einen schmalen Streifen. Senkrecht auf die Sonnenrichtung
    // wirft dasselbe Quad die volle Kronenflaeche.
    fwd = normalize(uCamPos - aCenter);
  } else {
    fwd = normalize(uCamTgt - uCamPos);
  }
  vec3 right = normalize(cross(fwd, vec3(0,1,0)));
  vec3 up = cross(right, fwd);
  // Quad in der Kameraebene, um rotY (aData.y) gedreht
  float cr = cos(aData.y), sr = sin(aData.y);
  vec2 ro = vec2(aOff.x * cr - aOff.y * sr, aOff.x * sr + aOff.y * cr);
  vec3 p = aCenter + right * (ro.x * aData.x) + up * (ro.y * aData.x);
  // Ferne Kronen bewegen sich mit demselben Boen-Feld wie Gras, Aeste und
  // Blaetter — sonst steht der Wald still, waehrend das Gras weht.
  vec2 gp = aCenter.xz * 0.10 - vec2(0.35, 0.20) * uTime;
  float gust = vnoise(gp * 2.3 + 7.7) * smoothstep(0.55, 0.85, vnoise(gp));
  float ph = hash12(floor(aCenter.xz / 15.0)) * 6.283;
  float wnd = sin(uTime * 1.8 + ph + aCenter.x * 0.35 + aCenter.z * 0.27);
  float amp = aData.x * (0.06 + 0.16 * gust);
  p.x += (wnd * 0.5 + gust) * amp;
  p.z += cos(uTime * 1.3 + ph * 1.7) * amp * 0.7;
  p.y += sin(uTime * 2.2 + ph) * amp * 0.35;
  float d = length(aCenter - uCamPos);
  // Uebergabe an die echten Einzelblaetter: leaf.vert duennt seine Blaetter um
  // iLodR +/-10 % statistisch aus — die Karten fuellen exakt komplementaer auf.
  // Der feste Schwellwert 20..28 m passte zu keiner CPU-Detailstufe: die Karten
  // waren unter 20 m ganz aus, die Blaetter hoerten schon viel frueher auf.
  // Im Schatten-Wurf ist uCamPos das Sonnenauge (63 m weg) -> dort immer 1.
  float near = (uLightFace > 0.5) ? 1.0 : smoothstep(iLodR * 0.90, iLodR * 1.10, d);
  // Build-Rand: Baeume werden bis RANGE=20 Zellen a 9 m gebaut (180 m halbe
  // Kantenlaenge, Ecken ~255 m). Bis 2026-10-07 endete das Blattdach bei 185 m,
  // die Staemme gingen aber bis ~255 m -> kahle Stummel in der Ferne.
  float far = 1.0 - smoothstep(190.0, 250.0, d);
  vFade = near * far * uTreeFade;
  if(uLightFace < 0.5 && vFade < 0.002){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
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
  vCol = iCol;
  gl_Position = uMVP * vec4(p, 1.0);
}
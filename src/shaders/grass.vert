#version 330 core
layout(location=0) in vec2 aBase;   // (t hoehe, w breite) Grundhalm
layout(location=1) in vec3 iPos;    // Halm-Basis (Welt)
layout(location=2) in vec4 iData;   // hoehe, breite, phase, schattierung
layout(location=3) in vec2 iData2;  // yaw, lean
layout(location=4) in float iBirth; // Birth-Time (fuer Fade-in)
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uHeightScale; // Halmhoehe-Skala (Slider)
uniform float uMaxDist;     // max. Gras-Sichtweite (Slider)
out vec3 vNrm;
out vec3 vPos;
out vec3 vCol;
out float vT;
out float vFade;
// hash12/vnoise: identisch zu fragment.glsl (Consistency-Regel)
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
  float ph = iData.z, sh = iData.w;
  float yaw = iData2.x, lean = iData2.y;
  float t = aBase.x, ww = aBase.y;
  float d = length(iPos - uCamPos);
  // Distanz-Skalierung (war CPU-seitig pro Build -> sprang beim Rebuild):
  // gleiche Formel wie der Dichte-Falloff (grass_cpu.c): groesser in der
  // Ferne, damit die Halme die Flaechen abdecken. Jetzt kontinuierlich zur
  // aktuellen Kameradistanz (aendert sich sanft beim Bewegen).
  float dens = pow(1.0 - smoothstep(8.0, uMaxDist, d), 1.6);
  float sizeScale = 1.0 + 1.8 * (1.0 - dens);
  float h = iData.x * uHeightScale * sizeScale, w = iData.y * sizeScale;
  // LOD: Halme wachsen in der Distanz zurueck (Terrain-Grasfarbe uebernimmt).
  // Dynamisch: Fade-out in den letzten 12m vor uMaxDist (Slider).
  float lod = 1.0 - smoothstep(uMaxDist - 12.0, uMaxDist, d);
  if(lod < 0.004){ gl_Position = vec4(2.0, 0.0, 0.0, 1.0); return; }
  float cy = cos(yaw), sy = sin(yaw);
  vec3 p = iPos;
  // Neigung: Spitze weicht in yaw-Richtung ab (t^2 = gekruemmter Halm).
  // Macht die Halme von oben sichtbar (projizierte Breite ~ w*sin(lean)).
  float lo = lean * t * t * h;
  p.x += cy * lo;
  p.z += sy * lo;
  p.y += t * h * lod * (1.0 - 0.3 * lean);
  // Wind: Basis-Brise (zwei Frequenzen) + wandernde Boen.
  // Boen = Noise-Feld im mitbewegten Koordinatensystem (Muster driftet in
  // Windrichtung) * sparsame Front (smoothstep) -> Boen ziehen ueber das
  // Feld und blenden sanft ein/aus.
  float wnd  = sin(uTime * 1.8 + ph + iPos.x * 0.35 + iPos.z * 0.27);
  float wnd2 = cos(uTime * 1.3 + ph * 1.7 + iPos.z * 0.41);
  vec2 gp = iPos.xz * 0.10 - vec2(0.35, 0.20) * uTime;
  float gust = vnoise(gp * 2.3 + 7.7) * smoothstep(0.55, 0.85, vnoise(gp));
  float wx = wnd * 0.11 + gust * 0.30;
  float wz = wnd2 * 0.08 + gust * 0.18;
  p.x += wx * t * t * lod;
  p.z += wz * t * t * lod;
  // Breite: zulaeufig zur Spitze, Ebene um yaw rotiert
  float taper = (1.0 - t) * (1.0 - t) * 0.5 + 0.06;
  vec2 woff = vec2(ww * w * taper, ww * w * taper * 0.7);
  p.x += woff.x * cy - woff.y * sy;
  p.z += woff.x * sy + woff.y * cy;
  // Normale: fast immer nach oben (wenig Wind/Yaw-Abhaengigkeit).
  // Vorher: vec3(-wnd*0.25, 1.0, -wnd2*0.18) -> Beleuchtung wechselte
  // zwischen hellgruen und dunkelgruen, je nach Wind/Yaw (sah aus wie
  // Halme, die ihre Farbe wechseln). Jetzt: stabile, helle Beleuchtung.
  vec3 n = vec3(-wx * 0.7, 1.0, -wz * 0.7);
  vNrm = normalize(vec3(n.x * cy - n.z * sy, n.y, n.x * sy + n.z * cy));
  vPos = p;
  vCol = vec3(sh, t, lod);
  vT = t;
  // Per-Halm-Fade-in: neue Halme (Birth-Time nahe uTime) blenden ueber 2s ein
  float age = uTime - iBirth;
  float fade = clamp(age / 2.0, 0.0, 1.0);
  vFade = fade * fade * (3.0 - 2.0 * fade);   // smoothstep [0,1]
  gl_Position = uMVP * vec4(p, 1.0);
}
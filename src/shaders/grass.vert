#version 330 core
layout(location=0) in vec2 aBase;   // (t hoehe, w breite) Grundhalm
layout(location=1) in vec3 iPos;    // Halm-Basis (Welt)
layout(location=2) in vec4 iData;   // hoehe, breite, phase, schattierung
layout(location=3) in vec2 iData2;  // yaw, lean
layout(location=4) in float iGrp;   // Fade-Gruppe (Index in uFadeGrp)
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uHeightScale; // Halmhoehe-Skala (Slider)
uniform float uMaxDist;     // max. Gras-Sichtweite (Slider)
uniform float uFadeDur;     // Fade-Dauer (musz gleich grassFadeDur sein, L-Taste)
uniform float uAngS0;       // Blickwinkel-Duennung: sin(theta)-Referenz (0 = aus)
uniform float uAngQ;        // Exponent der Winkel-Modulation
// Fade-GRUPPEN: (birth, death) pro Gruppe, death == 0 heisst "lebt". Birth und
// Death sind Fakten pro Build, nicht pro Halm — der Halm traegt beide Indizes
// gepackt in EINEM Float: iGrp = birthGrp + deathGrp * NGRP (deathGrp 0 = lebt).
// Ganzzahlig bis 2*9 Bit, also exakt in einem float darstellbar.
// Groesse muss GRASS_NGRP (scene.h) entsprechen.
#define NGRP 512   // MUSS GRASS_NGRP (scene.h) sein
uniform vec2 uFadeGrp[NGRP];
out vec3 vNrm;
out vec3 vPos;
out vec3 vCol;
out float vT;
out float vFade;
out float vCat;  // 0 = kein Override, 1 = Fade-in laeuft, 2 = Fade-out laeuft
out float vSide;   // Schritt 2: Breitenposition (ww: -0.5..0.5)
out vec3 vR;       // Schritt 2: Breitenrichtung (Halm-"rechts")
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
  // Blickwinkel-Duennung. Ein senkrechter Halm wird NICHT foerziert, eine
  // Wiese schon (Faktor sin theta). Flaechige Bedeckung = n*h*w/sin(theta),
  // konstante Pixeldichte verlangt also n ~ sin(theta). Gemessen ohne diese
  // Modulation: Coverage faellt von 9.4 (Blick -2 Grad) auf 4.9 (Blick -89
  // Grad), Halme/Pixel von 0.31 auf 0.001. Mit (sin/s0)^q, s0=0.35 q=0.7:
  // Coverage 5.1..5.2 ueber ALLE Blickwinkel, senkrechter Blick unveraendert.
  // Der Schwellwert ist pro Halm stabil aus seiner Position gehasht: beim
  // Kameradrehen duennen vorhandene Halme aus, es ploppen keine nach.
  float angFade = 1.0;
  if (uAngS0 > 0.0) {
    float st = (uCamPos.y - iPos.y) / max(d, 1e-3);   // sin(Erhebungswinkel)
    float P = pow(clamp(st / uAngS0, 0.0, 1.0), uAngQ);
    float rnd = fract(sin(dot(iPos.xz, vec2(12.9898, 78.233))) * 43758.5453);
    angFade = clamp((P + 0.15 - rnd) / 0.30, 0.0, 1.0);
    if (angFade <= 0.0) { gl_Position = vec4(2.0, 0.0, 0.0, 1.0); return; }
  }
  // Distanz-Skalierung (war CPU-seitig pro Build -> sprang beim Rebuild):
  // kontinuierlich zur AKTUELLEN Kameradistanz. Bewusst NICHT das Dichte-
  // Profil aus grass_cpu.c: das hat jetzt einen engen 4x-Nahfeld-Boost, und
  // darueber die Halmgroesse abzunehmen waere falsch (ss < 1 im Nahfeld).
  // 'sizeLod' ist nur die Groessenkurve: 1.0 vorn bis 2.8 in der Ferne.
  float sizeLod = pow(1.0 - smoothstep(8.0, uMaxDist, d), 1.6);
  float sizeScale = 1.0 + 1.8 * (1.0 - sizeLod);
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
  // Abgerundete Normale (Schritt 2): Halm ist kein flaches Band, sondern
  // leicht gewoelbt. vNrm = Flaechennormale F (horizontal, senkrecht zur
  // Breitenrichtung R) + kleine Wind-Stoerung; die Rundung ueber die Breite
  // (2k*vSide*R) und der Aufwaertsanteil u*Y kommen im Fragment-Shader
  // (k=0.6, u=0.5). F/R folgen aus der woff-Rechnung (0.7-Scherung).
  vec3 R = normalize(vec3(cy - 0.7 * sy, 0.0, sy + 0.7 * cy));
  vec3 F = vec3(-R.z, 0.0, R.x);
  vNrm = F + vec3(-wx * 0.7, 0.0, -wz * 0.7);
  vR = R;
  vSide = ww;
  vPos = p;
  vCol = vec3(sh, t, lod);
  vT = t;
  // Per-Halm-Fade: ein (Birth-Zeit der Gruppe) und aus (Death-Zeit der Gruppe),
  // je grassFadeDur (uFadeDur, Standard 1s), smoothstep. Vollstaendig
  // verblassene Halme cullen (Clip-Ebene x=2, wie der LOD-Cull).
  int gi = int(iGrp + 0.5);
  float birth = uFadeGrp[gi % NGRP].x;
  int di = gi / NGRP;
  float death = (di > 0) ? uFadeGrp[di].y : 0.0;
  float age = uTime - birth;
  float f = clamp(age / uFadeDur, 0.0, 1.0);
  f = f * f * (3.0 - 2.0 * f);
  if (death > 0.0) {
    float da = clamp((uTime - death) / uFadeDur, 0.0, 1.0);
    f *= 1.0 - (da * da * (3.0 - 2.0 * da));
  }
  if (f < 0.004) { gl_Position = vec4(2.0, 0.0, 0.0, 1.0); return; }
  // Kategorie-Codierung (C-Taste): GELB = laeuft aus, CYAN = laeuft ein,
  // 0 = KEIN Override (normale Grasfarbe). Ein Halm ist also nur waehrend des
  // laufenden Fades gefaerbt; ist der Fade durch, ist er normales Gras.
  vCat = (death > 0.0) ? 2.0 : ((age < uFadeDur) ? 1.0 : 0.0);
  vFade = f * angFade;
  gl_Position = uMVP * vec4(p, 1.0);
}
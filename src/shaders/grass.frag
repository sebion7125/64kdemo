#version 330 core
precision highp float;
in vec3 vNrm;
in vec3 vPos;
in vec3 vCol;
in float vT;
in float vFade;   // 0..1 (Fade-in nach Build)
in float vCat;    // 0 = kein Override, 1 = Fade-in laeuft, 2 = Fade-out laeuft
in float vSide;   // Schritt 2: Breitenposition (-0.5..0.5)
in vec3 vR;       // Schritt 2: Breitenrichtung
in float vSp;
in float vThick;     // Pflanzen-Typ aus grass.vert (0 Wiese, 1 Hochgras,
                  // 2 Lavendel, 3 Gaensebluemchen, 4 Schilf)
in vec4 vReg;     // Deckungsgrade der Region aus grass.vert
                  // (x Hochgras, y Lavendel, z Gaensebluemchen, w Schilf)
uniform vec3 uCamPos;
uniform float uDbgCat;   // C-Taste: 1 = Kategorie-Farbe als Albedo statt Grasfarbe
uniform float uAlbedoOnly; // 1 = nur Albedo ausgeben (Top-Down-Einfang)
out vec4 O;
const vec3 SUN = normalize(vec3(0.55, 0.38, -0.62));
const float WATER_Y = 0.25;   // Wasserstand (wie fragment.glsl/scene.h)
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
float fbm(vec2 p){
  float s = 0.0, a = 0.5;
  for(int i = 0; i < 4; i++){
    s += a * vnoise(p);
    p = p * 2.03 + vec2(17.1, 9.2);
    a *= 0.5;
  }
  return s;
}
vec3 skyColor(vec3 rd){
  float t = clamp(rd.y, 0.0, 1.0);
  vec3 col = mix(vec3(0.67, 0.78, 0.96), vec3(0.22, 0.42, 0.82), pow(t, 0.55));
  float sd = max(dot(rd, SUN), 0.0);
  col += vec3(1.0, 0.85, 0.6) * (pow(sd, 900.0) * 10.0 + pow(sd, 80.0) * 0.3 + pow(sd, 8.0) * 0.12);
  if(rd.y > 0.03){
    vec2 cuv = rd.xz / (rd.y + 0.12) * 0.4;
    float cl = smoothstep(0.48, 0.72, fbm(cuv));
    vec3 cc = mix(vec3(1.02, 1.00, 0.98), vec3(0.85, 0.87, 0.95), fbm(cuv * 2.7 + 3.1));
    cc *= 0.55 + 0.45 * pow(sd, 3.0);
    col = mix(col, cc, cl * 0.8 * smoothstep(0.03, 0.18, rd.y));
  }
  return col;
}
void main(){
  vec3 n = normalize(vNrm);
  // Abgerundete Normale (Schritt 2): ueber die Breite (vSide) kippt die
  // Normale zur Breitenrichtung vR (Halm wie leicht gewoelbtes Band), dazu
  // Aufwaertsanteil u (verhindert schwarze Halme). k=0.6, u=0.5 (Startwerte).
  // KEIN Double-Sided-Flip (dortflackern ueberlappender Haelme).
  n = normalize(n + 2.0 * 0.6 * vSide * vR + 0.5 * vec3(0.0, 1.0, 0.0));
  vec3 rd = normalize(uCamPos - vPos);
  // Grundfarbe aus dem Terrain (4-Oktaven-fbm, identisch zu fragment.glsl):
  // die Halm-Basis uebernimmt die lokale Gras-/Sandfarbe, die Spitze bleibt
  // hell (Silhouette gegen den Himmel). Gleiche Albedo-Werte wie terrainColor.
  float g = fbm(vPos.xz * 0.9);
  vec3 tGrass = mix(vec3(0.18, 0.46, 0.09), vec3(0.37, 0.66, 0.12), g) * vec3(0.87, 0.93, 0.89);
  float sandM = 1.0 - smoothstep(0.05, 0.40, abs(vPos.y - WATER_Y));
  vec3 sand = mix(vec3(0.62, 0.55, 0.38), vec3(0.72, 0.66, 0.48), fbm(vPos.xz * 1.5));
  vec3 base = mix(tGrass, sand, sandM);
  vec3 tip  = vec3(0.34, 0.64, 0.11);
  // Regionen-Toenung: die Deckungsgrade kommen als Varying aus grass.vert
  // (dort werden die Felder ohnehin fuer die Art bestimmt). Eigenes fbm pro
  // Fragment waere deutlich zu teuer. Muss zur Toenung in terrainColor passen,
  // sonst bricht die LOD-Uebergabe bei ~45 m.
  float wTall = vReg.x, wLav = vReg.y, wDaisy = vReg.z, wReed = vReg.w;
  base = mix(base, base * vec3(0.86, 0.94, 0.78), wTall * 0.55);   // Hochgras-Zone
  base = mix(base, vec3(0.34, 0.36, 0.50), wLav * 0.30);           // Lavendel-Schimmer
  base = mix(base, vec3(0.30, 0.44, 0.18), wDaisy * 0.35);         // Gaeensebluemchen-Wiese
  base = mix(base, vec3(0.20, 0.36, 0.16), wReed * 0.65);          // Schilfguertel
  // Art-Farben (vSp): Spitze bzw. Bluetenstand anders als der Halm
  if(vSp > 1.5 && vSp < 2.5){            // Lavendel: graugruener Stiel, violetter Stand
    base = mix(base, vec3(0.30, 0.36, 0.24), 0.45);
    tip  = mix(tip, vec3(0.44, 0.26, 0.64), smoothstep(0.40, 0.72, vT));
  } else if(vSp > 2.5 && vSp < 3.5){     // Gaensebluemchen: weisser Koerbchen-Rand
    tip  = mix(tip, vec3(0.93, 0.93, 0.88), smoothstep(0.55, 0.82, vT));
  } else if(vSp > 3.5){                  // Schilf: gruen wie die Baumblaetter,
    // Kolben ein weicher braenlich-gruenlicher Stand (kein strohiges Gelb)
    base = mix(base, vec3(0.16, 0.33, 0.13), 0.60);
    tip  = mix(tip, vec3(0.26, 0.38, 0.18), smoothstep(0.45, 0.66, vT));
    tip  = mix(tip, mix(vec3(0.40, 0.36, 0.24), vec3(0.50, 0.44, 0.30), vThick),
               smoothstep(0.66, 0.86, vT));
  } else if(vSp > 0.5 && vSp < 1.5){     // Hochgras: dunkler + gesaettigter
    base = base * vec3(0.88, 0.96, 0.80);
    tip  = mix(tip, vec3(0.26, 0.52, 0.10), 0.65);
  }
  // C-Taste: der Zustand wird als ALBEDO gefaerbt, aber nur WAHREND ein Fade
  // laeuft. Ist der Fade durch, ist der Halm normales Gras (base/tip unveraendert).
  // Beleuchtung, Transluzenz, Specular, Fog, Tonemapping und das Alpha-Blending
  // (vFade) bleiben unveraendert — man sieht das normale Gras-Rendering und darin
  // farbig codiert, wer gerade ein- bzw. ausgeblendet wird.
  //   GELB = laeuft aus, CYAN = laeuft ein
  // (Nicht Rot: im Nur-Gras-Modus ist der Hintergrund Magenta, und Rot ueber
  //  Magenta gemischt bleibt Magenta, solange der Fade laeuft.)
  if (uDbgCat > 0.5 && vCat > 0.5) {
    vec3 cat = (vCat > 1.5) ? vec3(1.00, 0.85, 0.05)   // laeuft aus
                            : vec3(0.10, 0.55, 1.00);  // laeuft ein
    base = cat; tip = cat;
  }
  // Arten-Modus (C zweimal): jede Art eine eigene Farbe. Damit ist die
  // Arten-Karte sichtbar UND zaehlbar: Pixel pro Farbe im Screenshot gegen die
  // CPU-Verteilung aus /tmp/test_species.c.
  if (uDbgCat > 1.5) {
    vec3 sc = vec3(0.15, 0.75, 0.15);                        // 0 Wiese
    if(vSp > 0.5 && vSp < 1.5)      sc = vec3(0.95, 0.90, 0.10);  // 1 Hochgras
    else if(vSp > 1.5 && vSp < 2.5) sc = vec3(0.75, 0.15, 0.90);  // 2 Lavendel
    else if(vSp > 2.5 && vSp < 3.5) sc = vec3(0.95, 0.95, 0.95);  // 3 Gaensebluemchen
    else if(vSp > 3.5)              sc = vec3(0.95, 0.45, 0.05);  // 4 Schilf
    base = sc; tip = sc;
  }
  vec3 albedo = mix(base, tip, vT * vT) * (0.75 + 0.5 * vCol.x);
  // Top-Down-Einfang: die Ferntextur wird als ALBEDO verwendet und im Terrain
  // ganz normal beleuchtet. Beleuchtung, Fog und Tonemapping duerfen hier nicht
  // einfließen, sonst werden sie spaeter doppelt angewandt.
  if (uAlbedoOnly > 0.5) { O = vec4(albedo, 1.0); return; }
  float dif = max(dot(n, SUN), 0.0) * 0.85 + 0.15;
  vec3 amb = vec3(0.40, 0.50, 0.68) * (0.45 + 0.55 * max(n.y, 0.0));
  vec3 col = albedo * (vec3(1.0, 0.92, 0.78) * dif + amb);
  // Transluzenz: Licht, das durch den Halm scheint (Gegenlicht).
  // Stark wenn die Kamera in Sonnenrichtung schaut (Blickrichtung ~ sun),
  // an der Spitze mehr als an der Basis (duennere Stelle).
  float tl = pow(max(dot(-rd, SUN), 0.0), 3.0) * (0.3 + 0.7 * vT);
  col += albedo * tl * 0.55;
  // Specular: weiches Glanzlicht (Blinn-Phong), an den Spitzen staerker
  // (duenneres Material reflektiert mehr). Dezent: Halme sind feiner als
  // ein Pixel, der Glanz ist ein Durchschnitt ueber mehrere Halme.
  vec3 hv = normalize(rd + SUN);
  float spec = pow(max(dot(n, hv), 0.0), 24.0) * (0.15 + 0.85 * vT * vT);
  col += vec3(1.0, 0.98, 0.9) * spec * 0.12;
  float s = length(vPos - uCamPos) - 60.0;   // Dunst wie fogAmt() in fragment.glsl
  float fog = s <= 0.0 ? 0.0 : 1.0 - exp(-s * s * 0.000012);
  col = mix(col, skyColor(rd), fog);
  // Tonemapping + Gamma (identisch zu fragment.glsl)
  col = 1.0 - exp(-col * 1.25);
  col = pow(col, vec3(0.4545));
  O = vec4(col, vFade);   // Alpha = Fade (neues Gras blendet ein)
}
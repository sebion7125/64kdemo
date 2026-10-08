#version 330 core
// Schilf-Pass (TODO 26). EINE Instanz = EINE Pflanze: Culm + 4 Blaetter an
// Knoten-Etagen + Kolben. Die Streifen haengen als TRIANGLE_STRIP mit
// entarteten Ecken aneinander (Template in main.c: reedBase[]).
//
//   aBase.x = Teil   0 = Culm, 1..4 = Blatt (Etage 0..3)
//   aBase.y = t      0..1 entlang des Teils
//   aBase.z = side   -0.5..0.5 quer
//
// Culm und Blaetter sind ZYLINDER-artige Baender: die Breitenrichtung steht
// senkrecht zur Blickrichtung (im Schatten-Wurf senkrecht zur Sonnenrichtung),
// damit ein Halm von jeder Seite eine Silhouette hat und nicht verschwindet.
layout(location=0) in vec3 aBase;
layout(location=1) in vec3 iPos;    // Fusspunkt (Welt)
layout(location=2) in vec4 iData;   // Hoeshe-Faktor, Breiten-Faktor [m], yaw, Phase
layout(location=3) in float iVar;   // Variante 0..1
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uReedH;      // eigene Hoehenskalierung (skaliert NICHT mit dem Gras-Slider)
uniform float uReedDens;   // Dichte-Faktor
uniform float uReedWide;   // Breiten-Faktor
uniform float uMaxDist;    // eigener Cull-Abstand (kein Gras-LOD, keine Ferntextur)
uniform float uLightFace;  // 1 = Schatten-Wurf: Streifen senkrecht zur Sonne
out vec3 vNrm;
out vec3 vPos;
out vec3 vCol;    // (Schattierung, t, lod) — wie grass.frag
out float vT;
out float vFade;
out float vRole;  // 1 = Culm, 2 = Blatt
out float vSide;
out vec3 vR;      // Breitenrichtung
out float vHead;  // 0..1 Anteil Kolben (nur Culm)
out float vVar;

const float WATER_Y = 0.25;
// hash12/vnoise/rgf/reedW: identisch zu grass.vert und fragment.glsl
// (Consistency-Regel: Schilf-Guertel am Boden und die Pflanzen muessen aus
// demselben Feld kommen).
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
float rgf(vec2 p){
  return (0.5 * vnoise(p) + 0.25 * vnoise(p * 2.03 + vec2(17.1, 9.2))) * 1.3333;
}
float reedW(float x, float z, float y){
  float dh = y - WATER_Y;
  if(dh > 0.45) return 0.0;
  float patch = rgf(vec2(x * 0.05 + 3.7, z * 0.05));
  return (1.0 - smoothstep(0.05, 0.45, dh)) * smoothstep(0.42, 0.60, patch);
}

void main(){
  float part = aBase.x, t = aBase.y, ww = aBase.z;
  float d = length(iPos - uCamPos);

  // Ob hier ueberhaupt eine Pflanze steht: gleiches Feld und derselbe Hash wie
  // im Gras-Pass (species() dort), damit die Rieden an derselben Stelle sind.
  float wReed = reedW(iPos.x, iPos.z, iPos.y) * 0.46 * uReedDens;
  if(hash12(vec2(iPos.x * 7.31, iPos.z * 11.7)) > wReed){
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return;
  }
  float hh = hash12(vec2(iPos.x * 0.91 + 7.7, iPos.z * 1.33));
  float th = hash12(vec2(iPos.x * 3.11, iPos.z * 5.17));
  float reedHMul = 0.55 + 1.05 * hh;              // pflanzenweise Groesse
  float hfac = iData.x, wfac = iData.y;
  float culmH = hfac * 3.20 * uReedH * reedHMul;  // 0,7-2,1 m (echt 1,5-4 m)

  float h, w, lean, bendPow, bendK, yw, baseY;
  const float HT0 = 0.55;   // Kolben beginnt bei 55 % Culm-Hoehe
  vRole = 1.0;
  if(part < 0.5){
    // Culm: duenn (echt 5-10 mm), fast senkrecht, Zylinder statt Trichter
    // 1.30 war die 3-fache Ueberhoehung aus dem Gras-Pass (dort musste ein
    // einzelnes Quad einen ganzen Halm spielen). Der Culm ist hier eine eigene
    // Flaeche, also echtes Mass: w = Halb-Breite, Band = 2w = 14-26 mm.
    h = culmH; w = wfac * uReedWide;
    lean = 0.02 + 0.20 * th; bendPow = 2.20; bendK = 1.00;
    yw = iData.z; baseY = 0.0;
  } else {
    // Blatt: sitzt an einer von vier Knoten-Etagen, steht ab und ueberhaengt
    float ti = floor(part - 0.5);
    float fl = 0.34 + 0.17 * ti;                  // 0,34/0,51/0,68/0,85 Culm-Hoehe
    h = hfac * (0.50 + 0.22 * th) * uReedH * reedHMul;
    w = wfac * (0.95 + 0.45 * th) * uReedWide;   // Blatt echt 8-25 mm breit
    lean = 0.90 + 0.45 * th; bendPow = 0.80; bendK = 1.55;
    yw = iData.z + ti * 1.9 + iVar * 2.4;
    baseY = fl * culmH;
    vRole = 2.0;
  }

  // Eigener Cull (KEIN Gras-sizeScale: Schilf behaelt echten Massstab).
  // Entfernungs-Uebergabe: frueher wurde die Pflanze mit lod auf Haehe 0
  // *geschrumpft* ("maeht" die Pflanzen weg, Nutzer). Jetzt bleibt die Geometrie
  // unveraendert stehen und lod steuert im Fragment eine Wuerfel-Ausduennung.
  float lod = 1.0 - smoothstep(uMaxDist - 12.0, uMaxDist, d);
  if(lod < 0.004){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }

  float cy = cos(yw), sy = sin(yw);
  vec3 p = iPos;
  p.y += baseY;
  // Neigung/Kruemmung: Spitze weicht in yaw-Richtung ab
  // Nickende Rispe: der obere Culm-Drittel weicht zusaetzlich zur Seite aus.
  float nod = (vRole < 1.5) ? 0.16 * h * pow(max(0.0, (t - HT0) / (1.0 - HT0)), 2.0) : 0.0;
  float lo = lean * pow(t, bendPow) * h * bendK + nod;
  p.x += cy * lo;
  p.z += sy * lo;
  p.y += t * h * (1.0 - 0.3 * lean);

  // Wind: Basis-Brise + wandernde Boen (Formeln wie grass.vert, damit sich
  // Schilf und Wiese gleich bewegen).
  float wnd  = sin(uTime * 1.8 + iData.w + iPos.x * 0.35 + iPos.z * 0.27);
  float wnd2 = cos(uTime * 1.3 + iData.w * 1.7 + iPos.z * 0.41);
  vec2 gp = iPos.xz * 0.10 - vec2(0.35, 0.20) * uTime;
  float gust = vnoise(gp * 2.3 + 7.7) * smoothstep(0.55, 0.85, vnoise(gp));
  float wx = wnd * 0.11 + gust * 0.30;
  float wz = wnd2 * 0.08 + gust * 0.18;
  p.x += wx * t * t;
  p.z += wz * t * t;

  // Breite: Culm als Zylinder (Auslaufen erst oben), Blatt mit schmaler Basis,
  // Maximum bei ~1/3, dann spitz.
  float taper;
  if(vRole < 1.5) taper = max(0.35, 0.95 - 0.85 * smoothstep(0.72, 1.0, t));
  else            taper = sin(3.14159 * pow(t, 0.62)) * 1.45;
  // Kolben: oberstes Culm-Drittel wird breiter (echt 15-50 cm buschiger Stand)
  float headOn = 0.0, headW = 0.0;
  if(vRole < 1.5){
    float ht = clamp((t - HT0) / (1.0 - HT0), 0.0, 1.0);
    // Rispe: unten schmal, breit erst in den oberen zwei Dritteln, dann zur
    // Spitze auslaufend (echt 15-30 cm lang, 20-40 cm buschig). Die symmetrische
    // Keule (7.00 * sin) sah aus wie ein Mordinstrument.
    headW  = (2.20 + 4.60 * ht) * sin(3.14159 * ht);
    headOn = smoothstep(HT0, HT0 + 0.12, t);
    taper = max(taper, headW);
  }
  vHead = headOn;

  // Breitenrichtung: senkrecht zur Blickrichtung (Schatten-Wurf: zur Sonne).
  vec2 wdir;
  if(uLightFace > 0.5){
    vec3 vd = normalize(iPos - uCamPos);
    wdir = normalize(vec2(-vd.z, vd.x) + vec2(1e-5));
  } else {
    // Culm und Kolben immer kamera-zugewandt (ein 3 cm duenner Halm ist von der
    // Seite sonst unsichtbar), Blaetter bleiben in ihrer eigenen yaw-Ebene.
    vec2 vd2 = normalize(vec2(-(iPos.z - uCamPos.z), (iPos.x - uCamPos.x)) + vec2(1e-5));
    wdir = mix(vec2(cy, sy), vd2, max(headOn, (vRole < 1.5) ? 1.0 : 0.0));
  }
  vec2 woff = vec2(ww * w * taper, ww * w * taper * 0.7);
  p.x += woff.x * wdir.x - woff.y * sy;
  p.z += woff.x * wdir.y + woff.y * cy;

  vec3 R = normalize(vec3(wdir.x - 0.7 * sy, 0.0, wdir.y + 0.7 * cy));
  vec3 F = vec3(-R.z, 0.0, R.x);
  vNrm = F + vec3(-wx * 0.7, 0.0, -wz * 0.7);
  vR = R;
  vSide = ww;
  vPos = p;
  vT = t;
  vVar = iVar;
  vCol = vec3(0.75 + 0.5 * iVar, t, lod);
  vFade = lod;
  gl_Position = uMVP * vec4(p, 1.0);
}

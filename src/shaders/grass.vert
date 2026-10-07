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
uniform float uCapture;     // 1 = Top-Down-Einfang fuer die Ferntextur
uniform float uFarFade;     // ab hier duennen die Halme zugunsten der Textur aus
uniform float uReedDens;    // DIAGNOSE/TUNING (DEMO_REED): Dichte-Faktor Schilf
uniform float uReedWide;    // DIAGNOSE/TUNING (DEMO_REED): Breiten-Faktor Schilf
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
out float vSp;
out vec2 vHead;   // x = 0..1 Kopf-Anteil, y = 0..1 entlang des Kopfes
out float vThick;
out float vReedRole;   // 0 = kein Schilf, 1 = Schilf-Culm, 2 = Schilf-Blatt   // 1 = dicker Schilfhalm mit grossem Kolben     // Pflanzen-Typ (0 Wiese, 1 Hochgras, 2 Lavendel,
                   // 3 Gaensebluemchen, 4 Schilf) — siehe species() unten
out vec4 vReg;     // Deckungsgrade der Region (Hochgras, Lavendel, Gaensebl.,
                   // Schilf) als Varying — grass.frag rechnet sie nicht selbst
const float WATER_Y = 0.25;   // wie fragment.glsl / scene.h / grass.frag
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
float fbm(vec2 p){
  float s = 0.0, a = 0.5;
  for(int i = 0; i < 4; i++){
    s += a * vnoise(p);
    p = p * 2.03 + vec2(17.1, 9.2);
    a *= 0.5;
  }
  return s;
}
// 2-Oktaven-Rauschen fuer die Pflanzen-Regionen: die Felder haben 20..90 m
// Wellenlaenge, die feinen Oktaven des 4-Oktaven-fbm bringen dort nichts — bei
// ~5 Mio. Vertex-Aufrufen pro Frame kosten sie aber viel (gemessen: mit 4
// Oktaven 2,0 -> 0,2 FPS in der VM). *1.3333 normiert auf 0..1 wie fbm, damit
// die Schwellwerte gleich bleiben. (Test /tmp/specmatch.c: die glatten Felder
// stimmen zwischen C und GLSL auf <1e-3 ueberein.)
float rgf(vec2 p){
  return (0.5 * vnoise(p) + 0.25 * vnoise(p * 2.03 + vec2(17.1, 9.2))) * 1.3333;
}
// Schilf/Rohricht: nur an der Wasserlinie, und dort in Rieden (Flecken von
// ~20 m Durchmesser) — nicht lueckenlos entlang des ganzen Ufers.
float reedW(float x, float z, float y){
  float dh = y - WATER_Y;
  if(dh > 0.45) return 0.0;   // Feld sparen: oberhalb der Uferzone nie Schilf
  float patch = rgf(vec2(x * 0.05 + 3.7, z * 0.05));
  return (1.0 - smoothstep(0.05, 0.45, dh)) * smoothstep(0.42, 0.60, patch);
}
// reg = Deckungsgrade der Region (Hochgras, Lavendel, Gaensebluemchen, Schilf).
// geht als Varying an grass.frag — dort darf es nicht nochmal gerechnet werden.
float species(float x, float z, float y, out vec4 reg){
  float dh = y - WATER_Y;
  // Schilf deutlich lichter als vorher: es stand in dichten Teppichen
  // (Nutzer: "zu dicht"). 0.45 laesst Uferbu schel statt Riegel.
  float wReed  = reedW(x, z, y) * 0.46 * uReedDens;   // wieder dichter (Nutzer)
  float wTall = 0.0, wLav = 0.0, wDaisy = 0.0, wPoppy = 0.0, wLupine = 0.0;
  if(dh >= 0.15){
    float nA = rgf(vec2(x * 0.011, z * 0.011));
    float nB = rgf(vec2(x * 0.031 + 5.3, z * 0.031));
    float nC = rgf(vec2(x * 0.019 + 11.1, z * 0.019));
    wTall  = smoothstep(0.44, 0.64, nC) * 0.40 * smoothstep(0.15, 0.60, dh);
    wLav   = smoothstep(0.45, 0.62, nA) * 0.30 * smoothstep(0.90, 1.80, dh);
    wDaisy = smoothstep(0.50, 0.68, nB) * 0.25 * smoothstep(0.15, 0.60, dh);
    // Blumen stehen in ARTREINEN Flecken (hohe Schwelle, schmales Band), nicht
    // als Zufallspunkte ueber die ganze Wiese verteilt.
    // Frequenz 0.05 (Noiszelle ~20 m). Bei 0.017..0.023 liegt das ganze Sichtfeld
    // in EINER Noiszelle, deren Wert hier weit unter der Schwelle lag
    // (gemessen rgf-Max 0.33 bei Schwelle 0.63) -> es gab nie Blumen.
    // EIN Feld, zwei Baelnder: ein zweites Feld mit eigenem Offset hatte eine
    // andere lokale Mittelzahl (gemessen 0.14 vs 0.54) und damit eine
    // Schwellweite, die regional nie erreichbar ist -> Lupine kam nirgends vor.
    float nD = rgf(vec2(x * 0.050 + 21.7, z * 0.050));
    // Kein Hohen-Gate: Blumen stehen auf jeder Wiese oberhalb der Wasserlinie.
    // (Mit Gate smoothstep(0.15,0.45,dh) waren im Nahfeld fast keine Blumen —
    // das Gelaende dort liegt bei dh ~0.20..0.30.)
    wPoppy  = smoothstep(0.55, 0.66, nD) * 0.55 * smoothstep(0.05, 0.20, dh);
    wLupine = smoothstep(0.455, 0.515, nD) * 0.12 * smoothstep(0.05, 0.20, dh);
  }
  reg = vec4(wTall, wLav, wDaisy, wReed);
  float r  = hash12(vec2(x * 7.31, z * 11.7));
  float acc = wReed;
  if(r < acc) return 4.0;
  // Blumen VOR Hochgras/Lavendel: sonst gewinnen die grossen Regionen fast immer
  // und auf einer Hochgraswiese steht keine einzige Blume.
  acc += wPoppy;  if(r < acc) return 5.0;
  acc += wLupine; if(r < acc) return 6.0;
  acc += wTall;  if(r < acc) return 1.0;
  acc += wLav;   if(r < acc) return 2.0;
  acc += wDaisy; if(r < acc) return 3.0;
  return 0.0;
}
void main(){
  float ph = iData.z, sh = iData.w;
  float yaw = iData2.x, lean = iData2.y;
  float t = aBase.x, ww = aBase.y;
  float d = length(iPos - uCamPos);
  // Art (Wiese/Hochgras/Lavendel/Gaensebluemchen/Schilf) — die EINZIGE Stelle,
  // an der Art-Effekte entstehen (Hoehe, Breite, Steifigkeit, Form, Farbe).
  // Die CPU rechnet die Art nicht mehr fuer die Geometrie: ein CPU/GPU-Vergleich
  // lief 3,3 % Abweichungen (Rundung), dann haette Halm-Hoehe und Halm-Farbe zu
  // verschiedenen Arten gehoert.
  float sp = species(iPos.x, iPos.z, iPos.y, vReg);
  float spH = 1.0, spW = 1.0, bendPow = 2.0, bendK = 1.0;
  float reedBase = 0.0, reedOff = 0.0, culmH = 0.0;
  vReedRole = 0.0;
  vThick = 0.0;
  if(sp > 0.5 && sp < 1.5){ spH = 1.75; spW = 1.35; lean *= 0.85; }   // Hochgras
  else if(sp > 1.5 && sp < 2.5){ spH = 1.35; spW = 0.55; lean *= 0.45; } // Lavendel
  else if(sp > 2.5 && sp < 3.5){ spH = 0.60; spW = 0.45; lean *= 1.20; } // Gaensebluemchen
  else if(sp > 4.5 && sp < 5.5){ spH = 1.55; spW = 0.42; lean *= 1.15; }  // Mohn: duenner Stiel
  else if(sp > 5.5){            spH = 1.45; spW = 0.50; lean *= 0.55; }  // Lupine: standfest
  else if(sp > 3.5){
    // Schilf (Phragmites australis). Recherche (Wikipedia, Beschreibung):
    //   Culm    : unverzweigter, gerader Halm, 1.5-4 m hoch, NUR 5-10 mm dick
    //   Blaetter: 18-60 cm lang, 1-3 cm breit, graugruen, setzen am oberen
    //             Halmdrittel an, stehen ab und ueberhaengen
    //   Kolben  : 15-50 cm langer, buschiger Stand ganz oben
    // Ein Instanz = ein Quad, also zwei Rollen pro Halm (per Hash):
    //   vReedRole = 1 -> Culm (duenn, gerade, hoch, Kolben oben)
    //   vReedRole = 2 -> Blatt (breit, kurz, erhoeht am Culm ansetzend, bogig)
    // Demo-Blattbreite ist gegen echtes Gras ~3x ueberhoeht (2.6-4.8 cm pro
    // Grashalm); Culm und Blatt werden relativ dazu angesetzt.
    float th = hash12(vec2(iPos.x * 3.11, iPos.z * 5.17));
    float rr = hash12(vec2(iPos.z * 2.13 + 5.5, iPos.x * 4.37));
    vReedRole = rr < 0.30 ? 1.0 : 2.0;
    if(vReedRole < 1.5){
      spH = 4.00;                              // Culm-Hoehe: 0.9-2.6 m
      spW = 0.95 * uReedWide;   // echt 5-10 mm, aber dann praktisch unsichtbar
                                                 // duenn (Nutzer: Halm nicht sichtbar)
      lean = 0.12; bendPow = 2.60; bendK = 0.70;  // fast gerade, nur leichte Biegung
      vThick = 1.0;                            // jeder Culm traegt einen Kolben
    } else {
      spH = 0.50 + 0.22 * th;                  // Blattlaenge: 0.11-0.46 m (kuerzer)
      spW = (1.90 + 0.70 * th) * uReedWide;    // 4.9-12.4 cm: breiter, nicht laenger
      lean = 0.90 + 0.45 * th;                 // steht ab und ueberhaengt
      bendPow = 0.80; bendK = 1.55;            // Kruemmung schwerpunktmaessig oben
      // Blaetter sitzen an Knoten: TIERS discrete Etagen ueber den Culm verteilt
      // (echte Halme: Blatthuelle in zwei Reihen entlang des Halmstaengs).
      float tr = hash12(vec2(iPos.x * 1.77 + 3.3, iPos.z * 2.91));
      float ti = floor(tr * 4.0);                    // 0..3 = Etage
      reedBase = 0.34 + 0.17 * ti + (th - 0.5) * 0.05;  // 0.34/0.51/0.68/0.85 Culm-Hoehe
      reedOff  = (th - 0.5) * (0.16 + 0.10 * ti);   // seitlich neben dem Culm
    }
  }
  // Blickwinkel-Duennung. Ein senkrechter Halm wird NICHT foerziert, eine
  // Wiese schon (Faktor sin theta). Flaechige Bedeckung = n*h*w/sin(theta),
  // konstante Pixeldichte verlangt also n ~ sin(theta). Gemessen ohne diese
  // Modulation: Coverage faellt von 9.4 (Blick -2 Grad) auf 4.9 (Blick -89
  // Grad), Halme/Pixel von 0.31 auf 0.001. Mit (sin/s0)^q, s0=0.35 q=0.7:
  // Coverage 5.1..5.2 ueber ALLE Blickwinkel, senkrechter Blick unveraendert.
  // Der Schwellwert ist pro Halm stabil aus seiner Position gehasht: beim
  // Kameradrehen duennen vorhandene Halme aus, es ploppen keine nach.
  float angFade = 1.0;
  if (uAngS0 > 0.0 && uCapture < 0.5) {
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
  float h = iData.x * uHeightScale * sizeScale * spH, w = iData.y * sizeScale * spW;
  if(vReedRole > 1.5) culmH = iData.x * uHeightScale * sizeScale * 4.00;
  // LOD: Halme wachsen in der Distanz zurueck (Terrain-Grasfarbe uebernimmt).
  // Der Uebergabe-Abstand kommt von uFarFade: ab dort uebernimmt die eingefangene
  // Top-Down-Textur, weil ein Halm unter ~2 px Bildbreite nur noch flackert.
  float farEnd = min(uMaxDist, uFarFade + 12.0);
  float lod = 1.0 - smoothstep(farEnd - 12.0, farEnd, d);
  if (uCapture > 0.5) { sizeScale = 1.0; lod = 1.0; }   // echter Massstab, kein Cull
  else if(lod < 0.004){ gl_Position = vec4(2.0, 0.0, 0.0, 1.0); return; }
  float cy = cos(yaw), sy = sin(yaw);
  vec3 p = iPos;
  // Neigung: Spitze weicht in yaw-Richtung ab (t^2 = gekruemmter Halm).
  // Macht die Halme von oben sichtbar (projizierte Breite ~ w*sin(lean)).
  float lo = lean * pow(t, bendPow) * h * bendK;
  p.x += cy * lo;
  p.z += sy * lo;
  p.y += t * h * lod * (1.0 - 0.3 * lean);
  if(vReedRole > 1.5){
    p.y += reedBase * culmH * lod;      // Blatt sitzt am oberen Culm, nicht am Boden
    p.x += cy * reedOff;
    p.z += sy * reedOff;
  }
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
  if (uCapture > 0.5) { wx = 0.0; wz = 0.0; }   // eingefroren, damit die Textur
                                                 // nicht waehrend des Einfangs waechst
  // KEIN Scheren: die schraege Einfang-Kamera (siehe ortho_oblique) bringt die
  // Seitenflaeche der Halme von selbst ins Bild.
  p.x += wx * t * t * lod;
  p.z += wz * t * t * lod;
  // Breite: zulaeufig zur Spitze, Ebene um yaw rotiert
  float taper = (1.0 - t) * (1.0 - t) * 0.5 + 0.06;
  if(vReedRole > 1.5){
    // Schilf-Blatt hat NICHT die Trichterform des Grashalms (unten am breitesten),
    // sondern schmale Basis, Maximum bei ~1/3, dann spitz auslaufend.
    taper = max(taper * 0.30, sin(3.14159 * pow(t, 0.62)) * 1.45);
  }
  // Bluetenstand / Schilf-Kolben: am oberen Ende wird der Halm wieder breiter
  // (Lavendel-Aehre, Gaensebluemchen-Koerbchen, Schilf-Kolben).
  float headOn = 0.0, headW = 0.0, headT0 = 1.0;
  // Wichtig: die Halmgeometrie ist bei t=0.55 am breitesten und laeuft zur Spitze
  // (t=1) auf die Breite NULL zu. Ein Kopf muss deshalb als BUCKEL um t~0.75
  // eingesetzt werden (max, nicht mix) — eine Verbreiterung am t=1-Vertex wäre
  // wirkungslos, dessen ww ist 0.
  // Der Kopf darf erst oberhalb der breiten Vertices (t=0.55) anfangen: die
  // Strip-Dreiecke darunter haetten sonst vHead>0.5 und der Silhouetten-Discard
  // frisst den halben Halm (gemessen: 45 % Mohn -> nur 354 Bildpunkte).
  const float HT0 = 0.55;
  float hampl = 0.0;
  if(sp > 1.5 && sp < 2.5)       hampl = 0.55f;                 // Lavendel-Aehre
  else if(sp > 2.5 && sp < 3.5)  hampl = 2.00f;                 // Gaensebluemchen (~2,5 cm)
  else if(sp > 4.5 && sp < 5.5)  hampl = 4.40f;                 // Mohn-Napf (~7 cm)
  else if(sp > 5.5)              hampl = 2.60f;                 // Lupinen-Stand (~4 cm)
  else if(sp > 3.5 && sp < 4.5)  hampl = (vReedRole < 1.5) ? 7.00f : 0.0f; // Kolben 15-50 cm
  if(hampl > 0.0){
    headT0 = HT0;
    float ht = clamp((t - HT0) / (1.0 - HT0), 0.0, 1.0);
    headW  = hampl * sin(3.14159 * ht);
    headOn = smoothstep(HT0, HT0 + 0.12, t);
  }
  taper = max(taper, headW);
  vHead = vec2(headOn, clamp((t - headT0) / max(1.0 - headT0, 1e-3), 0.0, 1.0));
  vec2 wdir = vec2(cy, sy);
  // Bluetenkoepfe und Bluetenstaende schauen die Kamera an (sonst sieht der
  // flache Kopf von der Seite aus wie eine Linie). Schilf/Lavendel bleiben aus.
  if(headOn > 0.02 && (sp < 1.5 || sp > 2.5) && sp < 4.5){
    vec3 vd = normalize(iPos - uCamPos);
    vec2 f = normalize(vec2(-vd.z, vd.x) + vec2(1e-5));
    wdir = mix(wdir, f, headOn);
  }
  vec2 woff = vec2(ww * w * taper, ww * w * taper * 0.7);
  p.x += woff.x * wdir.x - woff.y * sy;
  p.z += woff.x * wdir.y + woff.y * cy;
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
  vSp = sp;
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
  if (uCapture > 0.5) f = 1.0;   // Einfang unabhaengig vom laufenden Fade
  if (f < 0.004) { gl_Position = vec4(2.0, 0.0, 0.0, 1.0); return; }
  // Kategorie-Codierung (C-Taste): GELB = laeuft aus, CYAN = laeuft ein,
  // 0 = KEIN Override (normale Grasfarbe). Ein Halm ist also nur waehrend des
  // laufenden Fades gefaerbt; ist der Fade durch, ist er normales Gras.
  vCat = (death > 0.0) ? 2.0 : ((age < uFadeDur) ? 1.0 : 0.0);
  vFade = f * angFade;
  gl_Position = uMVP * vec4(p, 1.0);
}
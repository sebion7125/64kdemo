#version 330 core
layout(location=0) in vec3 aCenter;  // Quad-Mitte (Welt)
layout(location=1) in vec2 aOff;     // Ecke ±0.5
layout(location=2) in vec4 aData;    // size, rotY, seed, pad
layout(location=3) in float iLodR;   // Radius, ab dem diese Karte das Laub dieses
                                     // Baumes eretzt (CPU: 10x Wuchshoehe)
layout(location=4) in vec3 iCol;     // Blattfarbe DIESES Baum
layout(location=5) in float iLodR2;  // Schaltweite der groebsten Stufe (20x Wuchshoehe)
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform vec3 uCamTgt;
uniform float uTime;   // Wind (gleiches Boen-Feld wie Gras/Aeste/Blaetter)
uniform float uLightFace; // 1 = Quad steht senkrecht auf der Blickrichtung des
                          // Wurf-Betrachters (Sonne) — nur im Schatten-Pass.
uniform float uTreeFade = 1.0; // Ueberblendung beim LOD-Wechsel (Runde 27):
                               // 0 = dieser Satz verschwindet, 1 = voll sichtbaruniform float uTreeFade = 1.0; // Ueberblendung beim LOD-Wechsel (Runde 27):
                               // 0 = dieser Satz verschwindet, 1 = voll sichtbar
uniform float uTreeOld = 0.0;    // 1 = dieser Satz blendet AUS (der alte)
uniform float uCardsOn = 1.0;      // Schicht-Schalter (Tasten 4/5/6/7)
uniform float uBillboardOn = 1.0;
uniform vec3 uImpJit = vec3(0.0);   // Aufnahme: versetztes Wiederholen der Karten
uniform float uImpostor = 0.0;   // 1 = Aufnahme in die Impostor-Textur (keine
                                 // Entfernungsblenden, kein Wind, feste Kamera)
out vec2 vUV;
out vec3 vPos;
out float vSeed;
out float vFade;
out vec2 vTile;      // Atlas-Kachel (Spalte, Zeile)
out float vCrown;    // 1 = Ganzbaum-Kachel (tier 0) statt Buschel-Kachel
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
  // Aufnahme-Pass: dieselben Karten mehrmals versetzt gezeichnet fuellen die
  // Luecken von Arten mit wenigen grossen Karten (Kiefer: 162 Karten an einem
  // 7-m-Baum = einzelne Blatt-Lappen in der 128-px-Kachel).
  if(uImpostor > 0.5) p += uImpJit;
  // Ferne Kronen bewegen sich mit demselben Boen-Feld wie Gras, Aeste und
  // Blaetter — sonst steht der Wald still, waehrend das Gras weht.
  vec2 gp = aCenter.xz * 0.10 - vec2(0.35, 0.20) * uTime;
  float gust = vnoise(gp * 2.3 + 7.7) * smoothstep(0.55, 0.85, vnoise(gp));
  float ph = hash12(floor(aCenter.xz / 15.0)) * 6.283;
  float wnd = sin(uTime * 1.8 + ph + aCenter.x * 0.35 + aCenter.z * 0.27);
  // Das Ganzbaum-Billboard (Kachel >= 4) steht STILL: ein 20 m grosser,
  // mitschwingender Lappen "wabert" und wirkt albern (Nutzer). Die Blueten- und
  // Blattbewegung steckt in den Karten und Einzelblaettern.
  float amp = aData.x * ((aData.w >= 4.0) ? 0.0 : (0.06 + 0.16 * gust));
  p.x += (wnd * 0.5 + gust) * amp;
  p.z += cos(uTime * 1.3 + ph * 1.7) * amp * 0.7;
  p.y += sin(uTime * 2.2 + ph) * amp * 0.35;
  float d = length(aCenter - uCamPos);
  // Die drei Kronen-Stufen schliessen sich AUS und ueberblenden nur im Band:
  //   Einzelblaetter   bis 0,9 R1 ... aus bei 0,9..1,1 R1   (leaf.vert)
  //   Karten           ein bei 0,9..1,1 R1, aus bei 0,85..1,1 R2
  //   Baum-Billboard   ein bei 0,70..0,95 R2
  // Vorher (Runde 32) waren die Karten eine durchgehende Basis, die Blaetter
  // lagen DARUEBER -> nah sieht man die feinen Blaetter nicht, weil die groben
  // Karten dahinter die Krone fuellen (Nutzer: "quatsch"). Das Loch, das die
  // Karten bei R1 vorher rissen, kommt nicht wieder: R1 liegt jetzt bei 5x
  // Wuchshoehe, genau an der Stelle, an der die Einzelblaetter aufhoeren zu
  // decken (gemessen), und die Karten sind dichter (div 3 / scale 2,0).
  float near = (uLightFace > 0.5) ? 1.0 : smoothstep(iLodR * 0.85, iLodR * 1.00, d);
  // Uebergang Karten -> Ganzbaum-Billboard (Runde 29): in Stufe 2 liegen beide im
  // Puffer, das Billboard faellt ab 0,7 R2 ein, die Karten duennen bis 1,1 R2 aus.
  // Laeuft ueber die Entfernung, also kontinuierlich und unabhaengig davon, wann
  // der letzte Build passiert ist. (iLodR2 == 0: alter Generator -> ausblenden aus)
  if(uLightFace < 0.5 && iLodR2 > 0.0){
    // Karten aus 1.00..1.15 R2, Billboard ein 0.85..1.00 R2 — dasselbe
    // Ueberlappungsprinzip wie bei Blaettern/Karten: nie zwei halbe Schichten
    if(aData.w >= 4.0) near *= smoothstep(iLodR2 * 0.85, iLodR2 * 1.00, d);
    else               near *= 1.0 - smoothstep(iLodR2 * 1.00, iLodR2 * 1.15, d);
    // Schicht einzeln abschaltbar: near = 0 -> vFade = 0 -> Alpha 0 -> discard
    near *= (aData.w >= 4.0) ? uBillboardOn : uCardsOn;
  }
  // Aufnahme-Pass: keine Entfernungsschwelle, sonst verschwindet genau das,
  // was in die Kachel gehoert.
  if(uImpostor > 0.5) near = 1.0;
  // Build-Rand: Baeume werden bis RANGE=20 Zellen a 9 m gebaut (180 m halbe
  // Kantenlaenge, Ecken ~255 m). Bis 2026-10-07 endete das Blattdach bei 185 m,
  // die Staemme gingen aber bis ~255 m -> kahle Stummel in der Ferne.
  float far = 1.0 - smoothstep(190.0, 250.0, d);
  vFade = near * far * uTreeFade;
  // Beim Ueberblenden den alten Satz durchgaengig ausblenden (uTreeFade) statt
  // ihn unter 30 m hart wegucullen: das war genau bei nahen Baeumen ein Sprung
  // (dort ist der Uebergang am auffaelligsten). Doppeltes Zeichnen kostet nur
  // waehrend der 0,9 s Ueberblendung.
  if(uLightFace < 0.5 && vFade < 0.002){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  vUV = aOff * 2.0;
  vPos = p;
  vSeed = aData.z;
  // Kachel nach Blickrichtung: aData.w = Kronen-Variante (0..3), azIdx = eine von
  // 16 Aufnahmerichtungen. Die Krone parallaxiert also, wenn man sie umlaeuft.
  if(aData.w >= 4.0){
    // Ganzbaum-Impostor: 64 Kacheln in 8x8 (Art x Form), aufgenommen aus einer
    // Richtung. Ferne Baume sind 20-30 px, eine Richtung reicht dort.
    float ci = aData.w - 4.0;
    vTile = vec2(mod(ci, 8.0), floor(ci * 0.125));
    vCrown = 1.0;
  } else {
    float az = atan(aCenter.x - uCamPos.x, aCenter.z - uCamPos.z);
    float azIdx = floor(mod(az * (16.0 / 6.283185) + 8.0, 16.0));
    float t = aData.w * 16.0 + azIdx;
    vTile = vec2(mod(t, 8.0), floor(t / 8.0));
    vCrown = 0.0;
  }
  vNrm = normalize(uCamPos - aCenter);
  vRight = right; vUp = up;
  vCol = iCol;
  gl_Position = uMVP * vec4(p, 1.0);
}
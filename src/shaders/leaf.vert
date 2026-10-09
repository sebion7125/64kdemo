#version 330 core
layout(location=0) in vec2 aBase;   // (t laengs, w quer) Grundblatt
layout(location=1) in vec3 iPos;    // Blattbasis (Welt)
layout(location=2) in vec4 iData;   // size, rotY, tilt, lod-Pad
layout(location=3) in vec3 iCol;
layout(location=5) in vec2  iAnchor;  // Lotpunkt DIESES Baums — kanonischer LOD-Abstand
layout(location=6) in float iPres;    // Praesenz-Bits dieses Baums (LOD_PRES_*)
layout(location=4) in float iLodR;   // Radius, ab dem Karten dieses Baumes die
                                     // Einzelblaetter eretzen (CPU: 10x Wuchshoehe)
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uFocalPx = 0.0;   // Brennweite in Pixeln (fizzle-Zellgroesse)
uniform float uTreeFade = 1.0;   // 0 = dieser Satz verschwindet (Ueberblendung)uniform float uTreeFade = 1.0;   // 0 = dieser Satz verschwindet (Ueberblendung)
uniform float uTreeOld = 0.0;      // 1 = dieser Satz blendet AUS (der alte)
uniform vec3 uLodCam;   // Referenzpunkt fuer den Entfernungs-LOD (IMMER die
                        // echte Kamera — auch im Schatten-Wurf, sonst cullt der
                        // Wurf sich selbst die Blaetter weg)
uniform float uTime;
out vec3 vNrm;
out vec3 vPos;
out vec3 vCol;
out float vAlpha;
out float vFiz1, vFizInv1, vFizScale;   // fizzle (lod.glsl)
out float vT;
out float vW;      // quer ueber das Blatt (-0.5..0.5) -> Silhouette im Fragment-Shader
out float vAsp;    // Seitenverhaeltnis Breite/Laenge (Sapling: niemals 1)
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
#include "lod.glsl"   // kanonische Baelen — Spiegelbild von src/tree_lod.c

void main(){
  float size = iData.x, rotY = iData.y, tilt = iData.z;
  float t = aBase.x, w = aBase.y;
  float d = length(iAnchor - uLodCam.xz);  // LOD: waagerecht, Baum-Anker
  // LOD: die Kronen-Karten gibt es ab 0,9*iLodR, die echten Blaetter sterben bis
  // 1,1*iLodR statistisch aus — per Hash pro Blatt, damit die Dichte sanft faellt
  // und nicht schlagartig. Beide Seiten nutzen dieselbe Bande um iLodR, die
  // Ueberblendung ist also komplementaer (kein kahlstreifiger Uebergang).
  // Ueberblendung OHNE Deckungsloch (Nutzer: "pulsieren"): die abgehende
  // Schicht bleibt bis zum Ende des Bands voll an, die kommende blendet im
  // Band DAVOR hoch. Ueberlappung = beide voll, nie beide halb.
  // FIZZLE (lod.glsl): die abgehende Schicht behaelt die UNTERE Rauschmenge
  // (n < w). Zusammen mit den Karten (n >= 1-w, gleiches Rauschen, gleiche
  // Zellgroesse) ist die Flaeche durchgehend zu 100 % gedeckt.
  float keep = lodWLeaf(iLodR, d, iPres);
  // Ueberblendung beim LOD-Wechsel (Runde 27): der alte Satz lichtet sich pro
  // Blatt aus — gleiche Technik wie der Bande um iLodR, also ohne Alpha-Blend
  // und ohne Abhaengigkeit von der Draw-Reihenfolge.
  keep *= uTreeFade;
  // KEIN Hash-Cull pro Blatt mehr (Nutzer: "die Animation nimmt Blaetter
  // einzeln weg und baut sie neu auf ... alle Blaetter gleichzeitig alpha-
  // einblenden, und das Blending soll nicht gedithert sein"). Der Wert geht als
  // vAlpha ins Fragment und wird dort als echtes Alpha gemischt.
  if(uFizOn < 0.5) keep = (keep >= 0.5) ? 1.0 : 0.0;   // harte Umschaltung
  // COMPOSITE: Blaetter sind bei R1 die ABGEHENDE Stufe. Phase 1 zeichnet sie
  // im Band voll, Phase 2 zeichnet sie gar nicht (dort kommt die Kartenstufe).
  // uCompPhase: 1 = Basisdurchgang (Baeume in der Blende werden BAUMWEISE
  // kopiert, hier bleiben ihre Blaetter WEG — sonst wirft der Tiefentest die
  // kommende Stufe weg, gemessen: Box nach der kommenden Stufe genauso viele
  // helle Pixel wie vorher, 0 neue Fragmente).
  //             3 = nur die abgehende Stufe, volles Gewicht (Kopie-Puffer)
  //             2 = nie (die Blaetter sind die abgehende Stufe)
  if(uCompPhase > 0.5){
    if(uCompPhase < 1.5){ if(lodInBand(iLodR, d) > 0.5) keep = 0.0; }
    else if(uCompPhase < 2.5) keep = 0.0;
    else keep = 1.0;
  }
  vFiz1 = keep; vFizInv1 = 0.0;
  vFizScale = lodFizScale(uFocalPx, d);   // derselbe Anker-Abstand wie crown.vert
  if(keep <= 0.002) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  vAlpha = keep;   // nur noch Diagnose, die Blende laeuft ueber Discard
  // Groessen-Kompensation wie bei EZ-Tree: die uebrigbleibenden Blaetter werden
  // leicht groesser (deren leafScale 1,25 in LOD1), NICHT kleiner. Der alte
  // Faktor (1 - smoothstep(24,30,d)) schrumpfte jedes Blatt bis auf 0 — das war
  // ein zweiter, unabhaengiger Grund fuer "der Baum verliert seine Blaetter".
  float gs = 1.0 + 0.25 * lodBandIn(iLodR, d);
  // lokales Blatt: Laenge +X, Breite +Z, Normal +Y
  vec3 lp = vec3(t * size * gs, 0.0, w * size * gs);
  // Ueberhang (Rotation um Z): Spitze haeengt
  float ct = cos(tilt), st = sin(tilt);
  lp = vec3(lp.x * ct, -lp.x * st, lp.z);
  // leiser Biegung
  lp.y += t * t * size * 0.22;
  // Flattern des Einzelblatts (lokal, hohe Frequenz, staerker bei Boen)
  float ph = hash12(floor(iPos.xz / 15.0)) * 6.283;
  vec2 gp = iPos.xz * 0.10 - vec2(0.35, 0.20) * uTime;
  float gust = vnoise(gp * 2.3 + 7.7) * smoothstep(0.55, 0.85, vnoise(gp));
  lp.x += sin(uTime * 5.1 + ph * 3.7 + size * 91.0) * (0.10 + 0.40 * gust) * size;
  lp.y += cos(uTime * 4.3 + ph * 2.1 + size * 57.0) * (0.06 + 0.25 * gust) * size;
  // Drehung um Y
  float cr = cos(rotY), sr = sin(rotY);
  lp = vec3(lp.x * cr + lp.z * sr, lp.y, -lp.x * sr + lp.z * cr);
  vec3 p = iPos + lp;
  // Aest-Bewegung in WELT-Koordinaten: derselbe Ort wie in tree.vert -> dieselbe
  // Phase, Blatt und Ast schwingen also zusammen statt gegeneinander.
  float wnd  = sin(uTime * 1.8 + ph + iPos.x * 0.35 + iPos.z * 0.27);
  float wnd2 = cos(uTime * 1.3 + ph * 1.7 + iPos.z * 0.41);
  float swy = 0.55 + 0.45 * t;               // Spitze mehr als Blattbasis
  p.x += (wnd * 0.05 + gust * 0.17) * swy;
  p.z += (wnd2 * 0.04 + gust * 0.11) * swy;
  vec3 n = vec3(st * cr, ct, -st * sr);
  vNrm = n;
  vPos = p;
  vCol = iCol;
  vT = t;
  vW = w;
  vAsp = iData.w > 0.01 ? iData.w : 1.0;
  gl_Position = uMVP * vec4(p, 1.0);
}
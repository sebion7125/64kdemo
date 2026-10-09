#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec3 aCol;
layout(location=3) in vec3 aSW;   // sway, u (um das Rohr), v (Bogenlaenge)
layout(location=4) in float aR2;  // Schaltweite Ganzbaum-Billboard dieses Baums
layout(location=5) in vec2  aAnchor;  // Lotpunkt DIESES Baums
layout(location=6) in vec2  aLod;     // rad (Ast-Radius m), diss (Aufloes-Schwelle) — kanonischer LOD-Abstand
uniform float uMeshFade = 1.0;    // 0 = gar nicht ausblenden (Schattenwurf-Pass)
uniform float uBBPresent = 1.0;   // liegen Ganzbaum-Kacheln im Puffer?
uniform float uFocalPx = 0.0;     // Brennweite in Pixeln (1,7 * Bildhoehe)
out float vMeshFade;
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform float uTime;
out vec3 vNrm;
out vec3 vPos;
out vec4 vCol;
out vec2 vUV;
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
  vec3 p = aPos;
  // Wind: Phase pro Baum (grobe Zelle ~15m, ein Baum bleibt in einer Zelle).
  // Dasselbe Boen-Feld wie in grass.vert — Gras, Aeste, Blaetter und ferne
  // Kronen muessen auf dieselbe Boe reagieren, sonst wirkt der Wind falsch.
  float ph = hash12(floor(aPos.xz / 15.0)) * 6.283;
  float sw = aSW.x;
  vec2 gp = aPos.xz * 0.10 - vec2(0.35, 0.20) * uTime;
  float gust = vnoise(gp * 2.3 + 7.7) * smoothstep(0.55, 0.85, vnoise(gp));
  float wnd  = sin(uTime * 1.8 + ph + aPos.x * 0.35 + aPos.z * 0.27);
  float wnd2 = cos(uTime * 1.3 + ph * 1.7 + aPos.z * 0.41);
  p.x += (wnd * 0.05 + gust * 0.17) * sw;
  p.z += (wnd2 * 0.04 + gust * 0.11) * sw;
  vNrm = aNrm;
  vPos = p;
  vCol = vec4(aCol, aSW.x);
  vUV  = aSW.yz;
  // Stamm/Geruest blendet in demselben Band aus, in dem das Ganzbaum-Billboard
  // einblendet (0,85..1,00 R2): beide zeigen denselben Baum, gleichzeitig
  // gezeichnet waere doppelt (Nutzer: "Stamm und Ganzbaumbild nicht gleichzeitig").
  // Uebergabe in einem SEHR kurzen Band: ein weich ausblendendes Zweigwerk
  // gegen den Himmel sieht nach Rauschen aus (gemessen: graue Punkte ueber dem
  // halben Hintergrund). Das Billboard ist bei R2 schon voll, das Geruest geht
  // bei R2 — beide gleichzeitig sieht man damit nicht (Nutzer).
  // KANONISCHER LOD-ABSTAND (Strategie A): nicht der Vertex, sondern der
  // Lotpunkt des Baums. Vorher blendeten die Vertex eines Baums zu
  // verschiedenen Entfernungen aus, das Geruest konnte nie komplementaer zum
  // Billboard laufen (Spruenge beim Bewegen, Nutzerbefund).
  float md = length(aAnchor - uCamPos.xz);   // LOD: waagerecht, Baum-Anker
  // HOLZ-LOD NACH BILDGROESSE, OHNE TRANSPARENZ.
  // Ein Ast der Dicke 2r deckt px = 2*r*focalPx/d Pixel ab. Unter 0,6 px ist er
  // subpixelig, ueber 1,6 px voll aufloesbar; dazwischen liegt er auf. Aufgeloest
  // wird PRO AST (aLod.y ist ein stabiler Schwellwert aus dem Ast-Seed, derselbe
  // Wert auf allen Vertices des Astes): der Ast ist entweder ganz da (opak,
  // Depth-Write an) oder gar nicht. Damit gibt es im Holz keine Blende, keine
  // Sortierfrage und keine Z-Artefakte — und die Gesamtabdeckung sinkt sanft,
  // weil die duennsten Aeste zuerst wegfallen. Ein alpha-gemischtes Doppel
  // (alter vs. neuer Satz) waere nicht deckungserhaltend: es bliebe
  // a - a^2 f (1-f) uebrig, bei f = 0,5 bis zu 25 % weniger (gemessen:
  // Deckungs-Delle in JEDEM Ueberblend-Fenster).
  if(uFocalPx > 0.0){
    // Distanz = Lotpunkt des Baums (md), NICHT die Vertex-Position. Der
    // Schwellwert aLod.y sitzt pro ASTABSCHNITT auf allen Vertices gleich; mit
    // der Vertex-Entfernung kippte der Test innerhalb eines Abschnitts, ein
    // Teil der Ringe fiel weg und der Rest wurde als gestrecktes Viereck mit
    // Rindentextur sichtbar (Nutzerbefund "quadratische quads an zweigen,
    // seltsam gestreckte holztextur").
    if(aLod.y > lodBranchW(aLod.x, md, uFocalPx)){
      gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return;
    }
  }
  // Uebergabe ans Billboard im selben Band, in dem das Billboard einblendet
  // (0,90..1,10 R2): die Gewichte sind komplementaer, Summe immer 1.
  // ohne Billboard im Puffer bleibt das Geruest bis zur Bau-Grenze stehen
  float mf = lodWMesh(aR2, md, uBBPresent);
  // COMPOSITE Phase 2: das Geruest gehoert zur Basisstufe und wird nur in
  // Phase 1 gezeichnet — sonst staende es zweimal im Bild.
  if(uCompPhase > 1.5){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  // COMPOSITE Phase 2: das Geruest gehoert zur Basisstufe, es wird nur in
  // Phase 1 gezeichnet — sonst staende es zweimal im Bild.
  if(uCompPhase > 1.5){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  vMeshFade = uMeshFade * mf;
  // Cull am ENTFERNUNGS-Faktor, nicht am multiplizierten: im Schattenwurf-Pass
  // ist uMeshFade = 0, das haette das ganze Geruest aus der Schattenkarte
  // geworfen
  if(mf <= 0.002){ gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  gl_Position = uMVP * vec4(p, 1.0);
}
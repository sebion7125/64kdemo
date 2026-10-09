#version 330 core
precision highp float;
in vec3 vNrm;
in vec3 vPos;
in vec3 vCol;
in float vAlpha;
in float vFiz1, vFizInv1, vFizScale;
in float vT;
in float vW;
in float vAsp;
uniform vec3 uCamPos;
uniform float uTime = 0.0;   // Flattern (gleicher Name wie im Vertex-Shader -> gleiches Uniform)
uniform float uLeafMask;   // DEBUG: Blatt als Maske (reines Rot)
uniform float uDepthOnly = 0.0;
uniform float uAlbedoOnly; // Atlas-Kapture: nur Albedo + Abdeckung (keine Beleuchtung)
out vec4 O;
const vec3 SUN = normalize(vec3(0.55, 0.38, -0.62));

// ---------- Schattenwurf (Shadow-Map, eine orthogliche Licht-Kaskade) ----------
uniform sampler2D uShadow;
uniform mat4  uLightMVP;
uniform float uShadowOn;
uniform float uShadowPx;      // 1 / Aufloesung
uniform float uShadowBias;    // Bias in Depth-Einheiten (CPU: 1,5 Texel in m umgerechnet)

#include "lod.glsl"      // fizzle — dieselben Funktionen wie im Vertex-Shader
#include "foliage.glsl"  // gemeinsamer Laub-Shaderweg (Blatt- und Kronen-Pass)
// Rueckgabe: 1 = in Sonne, 0 = voll im Schatten.
// PCF: gefiltert werden die VERGLEICHSERGEBNISSE, nicht die Tiefen. GPU Gems 1
// Ch. 12 (Fernando/Everitt): "the comparison is performed at the four
// neighbouring shadow map samples, and the results are bilinearly filtered".
// Der alte Code mittelte neun DEPTH-Werte und verglich einmal -> hart, verschmiert
// und systematisch zu wenig Schatten.
void main(){
  // Masken-Diagnose ERST nach dem Discard der Blattsilhouette: sonst zaehlt die
  // Maske auch die leeren Quad-Ecken und die Farbmessung mittelt Hintergrund mit.
  // ---- Blattsilhouette (Vorlage: Sapling — Blätter sind in die Länge gestreckt,
  // niemals quadratisch; die Silhouette kommt aus der Textur/dem Shader, nicht
  // aus der Geometrie). vT = -0.5..0.5 entlang, vW = -0.5..0.5 quer.
  float tt = vT + 0.5;                                  // 0 Stiel .. 1 Spitze
  float hw = 0.5 * vAsp * pow(sin(3.14159 * pow(tt, 0.55)), 0.70);  // breiteste Stelle
  hw *= 1.0 - 0.07 * step(0.5, fract(tt * 17.0));       // feiner Sägeblattrand
  hw = max(hw, 1e-4);                                   // bei tt->0 wird hw klein
  float e = abs(vW) / hw;
  if(e > 1.0) discard;
  // FLATTER-Rauschen (Nutzer): Rascheln auch da, wo kein Einzelblatt mehr
  // aufloesbar ist. lodFizN ist in lod.glsl (dasselbe Rauschen wie die Blende),
  // hier mit anderer Zelle und eigenem Salz, damit Blende und Flattern
  // unabhaengig sind.
  {
    float fl = lodFizN(vPos, vFizScale * 0.5, 113.0 + floor(uTime * 3.0) * 0.71);
    e *= 1.0 / mix(1.0, 0.85 + 0.30 * fl, uFlut);
    if(e > 1.0) discard;
  }
  // Ueberblendung als echtes Alpha (nicht gedithert): die Silhouette bleibt
  // harte Kante (MSAA), die Ueberblendung multipliziert das Alpha.
  // Depth-only-Nachpass: siehe crown.frag
  // FIZZLE (lod.glsl): Uebergang durch Wegwerfen, nicht durch Mischen.
  if(uFizOn > 0.5){
    float n = lodFizN(vPos, vFizScale, 0.0);
    if(lodFizKeep(n, vFiz1, vFizInv1) < 0.5) discard;
  }
  if(uLeafMask > 0.5 && uLeafMask < 1.5){ O = vec4(1.0, 0.0, 0.0, 1.0); return; }
  float midrib = 1.0 - smoothstep(0.0, 0.055, abs(vW)); // Mittelrippe
  float vein   = 1.0 - smoothstep(0.0, 0.05,
                    abs(fract((tt + abs(vW) * 0.9) * 5.0) - 0.5) * 0.34 - abs(vW) * 0.02);
  vec3 n = normalize(vNrm);
  float pat = vnoise(vPos.xz * 16.0 + vPos.y * 9.0);
  vec3 albedo = vCol * (0.72 + 0.55 * pat);
  albedo *= 1.0 + midrib * 0.35 - vein * 0.10;          // Rippen heller, Adern dunkler
  albedo *= 1.0 - 0.18 * smoothstep(0.75, 1.0, e);      // Rand etwas dunkler
  if(uLeafMask > 1.5){ O = vec4(albedo, 1.0); return; } // DIAGNOSE: nur Albedo
  // DIAGNOSE (uLeafMask = 3): die Beleuchtungsfaktoren als Farbe, um Blätter und
  // Karten direkt vergleichen zu koennen: R = Sonne, G = Ambient, B = Transluzenz
  if(uAlbedoOnly > 0.5){ O = vec4(albedo, 1.0); return; }
  vec3 col = foliageLight(vPos, n, albedo, 0.85 + 0.15 * vT);
  // opak bei vAlpha = 1 (sonst wirkt das Laubdach duenn), echtes Alpha nur
  // waehrend der Ueberblendung — und die ist nicht gedithert
  // durchgehend, kein Sprung beim Start der Blende (begruendung wie crown.frag)
  O = vec4(col, 1.0);   // Blätter sind OPAK (fizzle, kein Alpha-Blend)
}
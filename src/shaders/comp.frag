#version 300 es
// COMPOSITE der zwei LOD-Stufen (Nutzerentwurf 2026-10-09).
//
// Problem, das dieser Pass loest: zwei halbtransparente Folien uebereinander
// ergeben NICHT die Mittelstufe (Deckung a - a^2 f (1-f), bis 25 % zu wenig bei
// f = 0,5), und ein Alpha-Blend haengt an der Zeichenreihenfolge. Deshalb werden
// die abgehende und die einkommende Stufe in JEDEIGEN Puffer opak gezeichnet
// (Alpha-Test, Tiefenpuffer) und erst hier gemischt.
//
// Regeln (covA/covB = die Stufe deckt diesen Pixel, erkannt an ihrer Tiefe
// gegenueber der Szenen-Tiefe VOR den Baeumen):
//   beide decken    -> lineare Mischung mit dem Gewicht aus texB.a
//   nur eine deckt  -> die, ohne Mischung  (kein Durchblick, kein Loch)
//   keine deckt     -> Szene unveraendert
// Ausgabe-Tiefe = min(dA, dB): was naeher ist, verdeckt in den Passen danach.
precision highp float;
precision highp sampler2D;

uniform sampler2D uTexA;     // Farbe des Szenen-FBO (enthaellt die abgehende Stufe)
uniform sampler2D uDepthA;   // Tiefe dazu
uniform sampler2D uTexB;     // Farbe der einkommenden Stufe, a = Blendengewicht
uniform sampler2D uDepthB;   // Tiefe dazu
uniform sampler2D uDepthS;   // Szenen-Tiefe VOR den Baeumen

out vec4 O;

void main(){
  ivec2 q = ivec2(gl_FragCoord.xy);
  vec4  A = texelFetch(uTexA, q, 0);
  vec4  B = texelFetch(uTexB, q, 0);
  float dA = texelFetch(uDepthA, q, 0).x;
  float dB = texelFetch(uDepthB, q, 0).x;
  float dS = texelFetch(uDepthS, q, 0).x;

  bool covA = dA < dS - 1e-6;
  bool covB = dB < dS - 1e-6;

  vec3 c = A.rgb;
  if(covB && covA)      c = mix(A.rgb, B.rgb, clamp(B.a, 0.0, 1.0));
  else if(covB)         c = B.rgb;
  O = vec4(c, 1.0);
  gl_FragDepth = min(dA, dB);
}

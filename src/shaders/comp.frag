#version 300 es
// Mischpass des Baum-Composites (Nutzerentwurf 2026-10-09).
//
// Ablauf pro Baum in main.c: Bildbox des Baums aus der Szene in fboB kopieren,
// die kommende LOD-Stufe DIESES Baums opak darueber zeichnen (Alpha-Test,
// Tiefentest, Tiefenwrite), dann ueber genau dieser Box mischen:
//
//     Bild = mischen(Szene, Kopie, w)      w = Gewicht der abgehenden Stufe
//
// Kein Alpha-Blend der Stufen uebereinander: zwei halbtransparente Folien
// ergeben nicht die Mittelstufe (Deckung a - a^2 f (1-f), bis 25 % zu wenig bei
// f = 0,5), und ein Blend haengt an der Zeichenreihenfolge.
//
// Warum keine Abdeckungs- oder Tiefen-Heuristik noetig ist: ausserhalb der
// Silhouette sind Szene und Kopie derselbe Hintergrund, das Mischen ist dort
// wirkungslos. Ueber Sichtbarkeit entscheidet allein der Tiefentest des opaken
// Draws; dieser Pass fuehrt weder Tiefentest noch Tiefenwrite.
precision highp float;

uniform sampler2D uTexA;   // Szene, wie sie JETZT steht (inkl. kommender Stufe)
uniform sampler2D uTexB;   // Kopie derselben Box VOR der kommenden Stufe
uniform float uW;          // Gewicht der ABGEHENDEN Stufe (1 = nur Kopie)

out vec4 O;

void main(){
  ivec2 q = ivec2(gl_FragCoord.xy);
  vec4 A = texelFetch(uTexA, q, 0);
  vec4 B = texelFetch(uTexB, q, 0);
  O = vec4(mix(A.rgb, B.rgb, clamp(uW, 0.0, 1.0)), 1.0);
}

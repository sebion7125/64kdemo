#version 300 es
// Mischpass des Baum-Composites (Nutzerentwurf 2026-10-09).
//
// Ablauf pro Baum in main.c: Bildbox des Baums aus der Szene in fboB kopieren
// (Farbe UND Tiefe), die abgehende LOD-Stufe DIESES Baums in die Kopie zeichnen,
// die kommende Stufe in die Szene zeichnen, dann ueber genau dieser Box die
// Kopie darueber blenden.
//
// Gemischt wird NICHT in einem dritten Puffer, sondern direkt mit dem
// Blend-Apparat: Quelle = Kopie, Ziel = Szene,
//   Mischung = Szene*(1-s) + Kopie*s      mit s = Anteil der Kopie
// als Blendfunktion (CONSTANT_ALPHA, ONE_MINUS_CONSTANT_ALPHA) und
// blendColor(a = s). Das spart gegenueber der frueheren Version den dritten
// Farb-Puffer und das Rueckkopieren der Box — gemessen 4 volle Pixel-Durchgaenge
// pro Baum und Frame statt 2 (bei 1280x720: 21 Baeume, 3 501 505 px Boxflaeche =
// 1520 % des Bildes pro Durchgang).
//
// Warum keine Abdeckungs- oder Tiefen-Heuristik noetig ist: ausserhalb der
// Silhouette sind Szene und Kopie derselbe Hintergrund, das Mischen ist dort
// wirkungslos. Ueber Sichtbarkeit entscheidet der Tiefentest des opaken Draws;
// dieser Pass fuehrt weder Tiefentest noch Tiefenwrite.
precision highp float;

uniform sampler2D uTexB;   // Kopie der Box, ueber die abgehende Stufe gezeichnet wurde

out vec4 O;

void main(){
  O = texelFetch(uTexB, ivec2(gl_FragCoord.xy), 0);
}

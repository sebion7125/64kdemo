// KANONISCHER LOD-KERN, GLSL-SEITE — Spiegelbild von src/tree_lod.c/.h.
//
// leaf.vert, crown.vert und tree.vert binden diese Datei per #include ein
// (tools/gen_shaders.sh bettet sie ein). Grund: dieselben Schichten desselben
// Baums muessen dieselben Baelen sehen. Vorher standen die Baelen dreimal
// handgeschrieben in drei Shadern und waren unterschiedlich (0,85..1,00 gegen
// 0,90..1,10) — die Gewichte konnten sich nie zu 1 ergaenzen, sichtbar als
// Spruenge und Pulsieren beim Bewegen.
//
// Bandbreite: LOD_BAND_LO/LOD_BAND_HI in src/tree_lod.h = 0,90..1,10 * R.
// tools/test_tree_lod.c prueft, dass hier dieselben Zahlen stehen.

// Knopfe (main.c, DEMO_FIZ / DEMO_FLUT):
//   uFizOn = 0 -> keine Rausch-Blende, harte Umschaltung bei Gewicht 0,5.
//     Grund Nutzerbefund 2026-10-09: "extremes Rauschen an der Ueberblendung,
//     sieht aus wie ein gruener Bienenschwarm". Ohne TAA (und A2C haben wir
//     verworfen) ist ein pixelgrosses binaires Muster sichtbar - die Illusion
//     von Alpha-Blend entsteht nur, wenn das Muster unter der Pixelgroesse
//     liegt und gemittelt wird.
//   uFlut = 0 -> kein Flatter-Rauschen auf der Silhouette (weisses Flimmern).
uniform float uFizOn = 1.0;
uniform float uFlut = 1.0;

// 0 auerhalb (nah) -> 1 jenseits des Bands: die kommende Schicht.
// Praesenz-Gating (Spiegelbild der LOD_PRES_*-Bits in src/tree_lod.h): pres
// sitzt pro Instanz und sagt, welche Schichten DIESES Baums wirklich im Puffer
// liegen. Der Build hinkt der Kamera hinterher; fehlt in der gezeichneten
// Haelfte die uebernehmende Schicht, muss die abgehende voll bleiben — sonst
// steht der Baum mitten im Blendenband ohne Laub da.
float lodBit(float pres, float b){ return mod(floor(pres / b), 2.0); }
float lodBandIn(float R, float d){ return smoothstep(R * 0.90, R * 1.10, d); }
// 1 bis zum Band -> 0 dahinter: die abgehende Schicht (Komplement zu lodBandIn).
float lodBandOut(float R, float d){ return 1.0 - smoothstep(R * 0.90, R * 1.10, d); }

// Gewichte. R1 = Einzelblaetter -> Karten, R2 = Karten/Geruest -> Ganzbaum.
// Invariante (vom Wirtstest geprueft): lodWLeaf + lodWCard + lodWBillboard == 1
// fuer jedes d, und lodWMesh + lodWBillboard == 1.
// bb = liegen Ganzbaum-Kacheln im Puffer (treeBillboard0). Ohne Kacheln duerfen
// Karten und Geruest bei R2 nicht ausblenden — sonst hat der ferne Baum gar keine
// Darstellung mehr (im Bild gemessen: ab ~110 m nichts mehr).
// Gewichte: klassisch komplementaer (Summe genau 1). Das ist die Voraussetzung
// fuer fizzle (unten): mit demselben Rauschen sind "n < w" und "n >= 1-w"
// disjunkt und vollstaendig. Der HALT (abgehend bleibt bis 0,97 des Bands 1)
// war ein Krueckstock gegen zwei halbtransparente Folien beim Alpha-Blend und
// ist nicht mehr noetig — er war ausserdem der Grund, dass die Blende nur ueber
// 0,70 m Kameraweg sichtbar war (gemessen).
float lodWLeaf(float R1, float d, float pres){
  return lodBit(pres, 2.0) > 0.5 ? lodBandOut(R1, d) : 1.0;   // ohne Karten: Blaetter bleiben
}
// Karten: ohne Einzelblaetter im Puffer muss das nahe Band voll stehen.
float lodWCardIn(float R1, float d, float pres){
  return lodBit(pres, 1.0) > 0.5 ? lodBandIn(R1, d) : 1.0;
}
float lodWCard(float R1, float R2, float d, float bb){ return lodBandIn(R1, d) * (bb > 0.5 ? lodBandOut(R2, d) : 1.0); }
float lodWBillboard(float R2, float d, float bb, float pres){
  if(bb < 0.5) return 0.0;
  // Gating: liegt ausser der Ganzbaum-Kachel nichts im Puffer, muss sie auch
  // nah voll stehen — sonst hat der Baum ueberhaupt keine Laubdarstellung.
  if(pres > 0.5 && lodBit(pres, 1.0) < 0.5 && lodBit(pres, 2.0) < 0.5) return 1.0;
  return lodBandIn(R2, d);
}
float lodWMesh(float R2, float d, float bb){ return bb > 0.5 ? lodBandOut(R2, d) : 1.0; }


// ---- FIZZLE: Ueberblendung durch Wegwerfen statt Mischen ----
// Nutzerentscheid 2026-10-09. Referenz: SpeedTree-Renderer, GPU Gems 3 Kap.
// 4.6.2 "Level-of-Detail Cross-Fading": "The fizzle uses alpha test to produce
// a cross-fade at the pixel level." Beide Schichten bleiben OPAK und schreiben
// Tiefe. Damit entfaellt das Folien-Problem (1-(1-w)^2 Deckung), die
// Zeichenreihenfolge ist egal (pro Pixel gewinnt der Tiefentest), und
// schneidende Quadrate (tier 2) sowie ueberlappende Baeume sind korrekt — ohne
// Sortieren und ohne Tiefen-Nachpass.
// Regel (dasselbe Rauschen n, komplementaere Gewichte w_ab + w_komm = 1):
//   abgehend:  behalten wo  n <  w
//   kommend:   behalten wo  n >= 1 - w
// Die Mengen sind disjunkt und ihre Vereinigung ist die ganze Flaeche.
float lodHash12(vec2 p){
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
// Zellgroesse = ein Bildpunkt auf dieser Entfernung. Das Muster klebt am Baum
// (Weltzellen); ein bildschirmfestes Muster wuerde bei Kamerabewegung flimmern.
float lodFizScale(float focalPx, float d){ return clamp(focalPx / max(d, 0.01), 1.0, 512.0); }
float lodFizN(vec3 p, float s, float salt){
  vec2 q = floor(p.xz * s) + floor(p.y * s) * vec2(13.7, 7.3) + vec2(salt, salt * 0.37);
  return lodHash12(q);
}
float lodFizKeep(float n, float w, float inv){
  return (inv < 0.5) ? ((n < w) ? 1.0 : 0.0) : ((n >= 1.0 - w) ? 1.0 : 0.0);
}
// Ast-Aufloesung nach BILDGROESSE — Spiegelbild von treeLodWBranch/treeLodRadCut
// (Konstanten 0,6/1,6 Pixel; tools/test_tree_lod.c vergleicht beide Seiten).
float lodBranchW(float rad, float d, float focalPx){
  return smoothstep(0.6, 1.6, 2.0 * rad * focalPx / max(d, 0.01));
}

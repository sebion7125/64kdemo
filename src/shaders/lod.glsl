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

// 0 auerhalb (nah) -> 1 jenseits des Bands: die kommende Schicht.
float lodBandIn(float R, float d){ return smoothstep(R * 0.90, R * 1.10, d); }
// 1 bis zum Band -> 0 dahinter: die abgehende Schicht (Komplement zu lodBandIn).
float lodBandOut(float R, float d){ return 1.0 - smoothstep(R * 0.90, R * 1.10, d); }

// Gewichte. R1 = Einzelblaetter -> Karten, R2 = Karten/Geruest -> Ganzbaum.
// Invariante (vom Wirtstest geprueft): lodWLeaf + lodWCard + lodWBillboard == 1
// fuer jedes d, und lodWMesh + lodWBillboard == 1.
// bb = liegen Ganzbaum-Kacheln im Puffer (treeBillboard0). Ohne Kacheln duerfen
// Karten und Geruest bei R2 nicht ausblenden — sonst hat der ferne Baum gar keine
// Darstellung mehr (im Bild gemessen: ab ~110 m nichts mehr).
// Abgehende Schicht mit HALT: sie bleibt opak (1) bis t = 0,9 des Bands und
// faellt erst im letzten Zehntel auf 0. Grund (Nutzerbefund): zwei
// halbtransparente Folien uebereinander — abgehend (1-w) ueber kommend (w) —
// lassen den Hintergrund durchscheinen, die Krone wirkt wie zwei Folien. Mit
// opakem Grund und linear hochblendender kommender Schicht ist die Abdeckung
// durchgehend 1 und die Farbe ein linearer Uebergang. Das letzte Zehntel ist
// noetig, damit die abgehende Schicht nicht hart abschaltet, wenn die kommende
// schon voll ist; dort ist die kommende aber bereits >= 0,9, also keine Folie.
float lodBandOutHold(float R, float d){
  float t = smoothstep(R * 0.90, R * 1.10, d);
  return 1.0 - smoothstep(0.97, 1.0, t);   // Auslauf = 3 % des Bands
}
float lodWLeaf(float R1, float d){ return lodBandOutHold(R1, d); }
float lodWCard(float R1, float R2, float d, float bb){ return lodBandIn(R1, d) * (bb > 0.5 ? lodBandOut(R2, d) : 1.0); }
float lodWBillboard(float R2, float d, float bb){ return bb > 0.5 ? lodBandIn(R2, d) : 0.0; }
float lodWMesh(float R2, float d, float bb){ return bb > 0.5 ? lodBandOutHold(R2, d) : 1.0; }

// Ast-Aufloesung nach BILDGROESSE — Spiegelbild von treeLodWBranch/treeLodRadCut
// (Konstanten 0,6/1,6 Pixel; tools/test_tree_lod.c vergleicht beide Seiten).
float lodBranchW(float rad, float d, float focalPx){
  return smoothstep(0.6, 1.6, 2.0 * rad * focalPx / max(d, 0.01));
}

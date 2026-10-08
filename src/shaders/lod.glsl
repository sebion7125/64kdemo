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
float lodWLeaf(float R1, float d){ return lodBandOut(R1, d); }
float lodWCard(float R1, float R2, float d){ return lodBandIn(R1, d) * lodBandOut(R2, d); }
float lodWBillboard(float R2, float d){ return lodBandIn(R2, d); }
float lodWMesh(float R2, float d){ return lodBandOut(R2, d); }

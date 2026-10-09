#version 330 core
precision highp float;
in vec2 vUV;
in vec3 vPos;
in float vSeed;
in float vFade;
in float vCompW;   // Composite: Gewicht der einkommenden Stufe
in float vFiz1, vFiz2, vFizInv1, vFizInv2, vFizScale;
in vec2 vTile;      // Atlas-Kachel (Spalte, Zeile)
in float vCrown;      // 1 = Ganzbaum-Kachel statt Buschel-Kachel
in vec3 vNrm;       // Billboard-Normale
in vec3 vRight, vUp;
in vec3 vCol;       // Blattfarbe des Baumes (Atlas-Kachel ist farbneutral)
uniform vec3 uCamPos;
uniform sampler2D uFoliage;
uniform float uDepthOnly = 0.0;
uniform float uImpostor = 0.0;   // 1 = Aufnahme-Pass
uniform sampler2D uCrownTex;   // 2x2 Ganzbaum-Kronen-Kacheln (tier 0)   // Atlas: 8x8 Kacheln echter Blattwerk-Aufnahmen
uniform float uTime = 0.0;   // Flattern (dasselbe Uniform wie im Vertex-Shader)
uniform float uCrownMask;   // DEBUG: Kronen-Quads als reines Rot -> exakte Maske
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
  // Atlas-Kachel antasten. Ohne Mipmaps (die Kacheln wuerden sich beim Mip-
  // Wechsel aufloesen) -> Aufloesungsstufe selbst schaetzen und die UV an den
  // Kachelrand zurueckziehen, damit nichts in die Nachbar-Kachel blutet.
  vec2 cell = vUV * 0.5 + 0.5;
  vec2 tuv = (vTile + cell) * (1.0 / 8.0);
  float foot = max(length(fwidth(tuv)), 1e-7);
  float lod = clamp(log2(foot * 2048.0), 0.0, 3.0);
  float inset = exp2(lod) * 0.5 / 256.0;
  cell = clamp(cell, inset, 1.0 - inset);
  vec4 tx = textureLod(uFoliage, (vTile + cell) * (1.0 / 8.0), lod);
  // Ganzbaum-Kachel (groebste Detailstufe): andere Textur, 2x2 Kacheln a 128 px
  if(vCrown > 0.5){
    // Der Impostor-Atlas hat KEINE Gutters zwischen den Kacheln: ab lod 1
    // vermischt die Mip-Kette Nachbar-Kacheln, und im Wald erscheinen harte
    // gruenue Rechtecke aus der Nachbarkrone. Deshalb hier bei lod 0/1 bleiben.
    // Rand des Billboards: siehe unten bei der Alpha-Berechnung (emEdge).
    // Mip-Kette liegt jetzt PRO KACHEL vor (build_tile_mips in main.c); die
    // alte Begrenzung auf lod 0/1 war nur noetig, weil die Treiber-Mip-Kette
    // Nachbar-Kacheln vermischt haette.
    // Hoehere Stufen als 2 verteilen die Kachel-Alpha ueber IHRE ganze Flaeche
    // (gemessen: Deckung 20,9 %, Alpha-Mittel 52 — bei lod 3 ist die halbe
    // Kachel > 0.02). Zusammen mit dem Alpha-1-Ausgabe unten war das Ergebnis
    // ein undurchsichtiges Rechteck ums Ganzbaum-Billboard (Nutzerbefund
    // "schwarz wo transparent sein sollte", Bild /tmp/q_100_bb.png).
    // Der Aufloesungsbedarf wird in ATLAS-Texeln gemessen: der Karten-Atlas ist
    // 2048 px gross (lod oben), der Impostor-Atlas nur 1024. Mit 2048 rechnete
    // die Kachel eine Stufe zu scharf gefiltert (Beispiel 100 m, Quad 110 px:
    // richtig lod 1,2, berechnet 2,2) -> die Alpha ueber die ganze Kachel
    // verschmiert.
    float clod = clamp(log2(foot * 1024.0), 0.0, 2.0);
    // Einzug um eine HALBE TEXEL DER KACHEL (128 px), nicht des ganzen Atlas:
    // der alte Wert (0.5/1024) war 8x zu klein -> LINEAR mischt an der Kachel-
    // kante die Nachbarkachel rein, und die ist am Rand schwarz (gemessen: 64
    // von 64 Kacheln mit komplett schwarzem Rand) -> schwarzer Rand.
    float cinset = exp2(clod) * 0.5 / 128.0;
    vec2 cc = clamp(cell, cinset, 1.0 - cinset);
    tx = textureLod(uCrownTex, (vTile + cc) * (1.0 / 8.0), clod);
  }
  // Harte Polygonkanten an fernen Kronen (alter Nutzerbefund): mit steigender
  // Distanz wird die UV in die Kachelmitte zurueckgezogen, dadurch fuellt die
  // Blattmasse das ganze Quad und seine gerade Begrenzung wird sichtbar.
  // Gegenmittel: (a) Alpha am Quad-Rand ueber ~1,5 Bildpunkte auslaufen lassen,
  // (b) den Schwellwert pro Pixel wuerfeln, damit MSAA daraus eine weiche Kante
  //     macht statt einer harten Linie.
  float band = max(foot * 1.6, 1e-5);
  float e = min(min(cell.x, 1.0 - cell.x), min(cell.y, 1.0 - cell.y));
  // Der Fade muss auf die ALPHA, nicht auf die Schwelle: der alte Test
  // `a < thr * vFade` macht die Schwelle bei vFade = 0 exakt NULL -> gar kein
  // Discard mehr. 15,3 % aller Kronen-Quads liegen aber bei d >= 185 m und haben
  // vFade = 0 (gemessen, /tmp/crowndist.c): deren transparenten Atlas-Bereiche
  // (rgb = 0) wurden als dunkles Quadrat gemalt = der Nutzerbefund "schwarz".
  float a = tx.a * smoothstep(0.0, band, e);
  // FLATTER-Rauschen (Nutzer: "das Einzelrascheln sieht man als Pixelrauschen,
  // das sollte da sein"). Verankert an WELTZELLEN von etwa zwei Bildpunkten und
  // zeitlich gestaffelt: die Silhouette jeder Zelle flackert, statt dass das
  // Muster ueber die Flaeche laeuft (UV-Versatz wirkt schnell wie Wasser).
  // Wirkt nur an Werten nahe der Silhouetten-Schwelle, die Blattflaeche selbst
  // bleibt unveraendert.
  {
    float fl = lodFizN(vPos, vFizScale * 0.5, 113.0 + floor(uTime * 3.0) * 0.71);
    a *= mix(1.0, 0.85 + 0.30 * fl, uFlut);
  }
  float cov = a;   // Deckung vor der Ueberblendung (die kommt als vFade dazu)  // Rand des Billboards ausblenden: entkommt der Baum (Aeste, Blaetter) der
  // BBox-Schaetzung, beruehrt der Kachelinhalt den Rand — im Wald sieht man dann
  // ein hartes gruenes Rechteck gegen den Himmel (gemessen: 15 von 64 Kacheln
  // randberuehrend; mit DEMO_NOBB0=1 verschwindet der Block).
  if(vCrown > 0.5){
    float em = max(abs(cell.x * 2.0 - 1.0), abs(cell.y * 2.0 - 1.0));
    a *= 1.0 - smoothstep(0.88, 1.0, em);
  }
  cov = a * vFade;
  // Die raeumliche Kante des Blattbuschels bleibt wie sie war (Schwelle, per
  // MSAA weich) — sie macht die Krone fest. GEDITHERT wird nicht mehr die
  // UEBERBLENDUNG (Nutzer: "das Blending soll nicht gedithert sein"): vFade
  // geht als echtes Alpha ins Ergebnis statt in die Schwelle.
  // KEIN zufälliger Schwellwert mehr (alt: thr = 0.5 + (hash(gl_FragCoord)-0.5)
  // * 0.75): der war pro Pixel unterschiedlich und abhaengig von der
  // Bildschirmposition -> die Kronensilhouette schimmerte bei Kamerabewegung.
  // Die Texturalpha selbst ist die weiche Kante, gemischt wird mit echtem Alpha.
  // Depth-only-Nachpass: Schwelle am SCHICHTGEWICHT vFade, nicht am pro-Pixel
  // cov — sonst kippt die Tiefenwrite mitten in der Blende hart um (Sprung).
  // OPAK + FIZZLE (lod.glsl): die raumliche Kante ist eine harte Schwelle, der
  // Uebergang zwischen den Schichten ist Wegwerfen. Kein Alpha-Blend ->
  // reihenfolge-unabhaengig, schneidende Karten korrekt, Tiefenpuffer stimmt.
  if(vCrown < 0.5){
    if(a < 0.5) discard;
    if(uFizOn > 0.5){
      float n1 = lodFizN(vPos, vFizScale, 0.0);
      float n2 = lodFizN(vPos, vFizScale, 57.0);
      if(lodFizKeep(n1, vFiz1, vFizInv1) < 0.5) discard;
      if(lodFizKeep(n2, vFiz2, vFizInv2) < 0.5) discard;
    }
  }
  // Schwelle schichtabhaengig: die Karten sind duenn gesäte Buschel und
  // brauchen eine niedrige Schwelle; die Ganzbaum-Kachel hat eine grosse
  // zusammenhaengende Flaeche, bei der ein niedriger Wert die ganze Kachel
  // durchlaesst (Rechteck).
  else if(cov < 0.02) discard;
  // Masken-Diagnose erst nach dem Discard: sonst zaehlt sie auch die leeren
  // areas der Kachel und die Farbmessung mittelt den Hintergrund mit.
  if(uCrownMask > 0.5 && uCrownMask < 1.5){ O = vec4(1.0, 0.0, 0.0, 1.0); return; }
  // Beleuchtung wie im Blatt-Shader, nicht als Kugel: die Karte steht fuer einen
  // Buschel duenner Blaetter, also dieselbe Beleuchtung (beidseitig diffus,
  // Transluzenz, warmes Ambient). Vorher eigene Kugel-Schattierung mit 0,45
  // Diffus-Anteil -> Karten deutlich heller/grauer als das Nah-LOD (gemessen am
  // selben Baum: 60 m Karten (128,148,116) gegen 16 m Einzelblaetter (59,99,39)).
  // Ganzbaum-Kachel: sie enthaelt das bereits beleuchtete, getonempte Bild des
  // Baues (die Aufnahme-Paesse laufen durch dieselben Shader). Noch einmal
  // Beleuchtung + Tonemapping + Gamma obendraefte machte daraus ein graues
  // Rechteck (Bild /tmp/s_100.png).
  if(vCrown > 0.5){ O = vec4(tx.rgb, cov); return; }
  vec3 nrm = normalize(vNrm + (vRight * vUV.x + vUp * vUV.y) * 0.55);
  // Beleuchtung ueber foliageLight = exakt derselbe Weg wie in leaf.frag.
  // Vorher zwei Kopien; die Transluzenz hing in einem Shader von der
  // Blickrichtung ab, im anderen von der Normalen.
  // Die Kachel ist ein farbneutraler BLATTBUSCHEL (mehrere unserer Blaetter an
  // kurzen Stielen) — dieselbe Silhouette wie das Nah-LOD. Die Farbe kommt pro
  // Baum aus der Instanz, vorher steckte eine zufaellige Fremdfarbe in der Textur.
  vec3 albedo = tx.rgb * vCol;
  if(uCrownMask > 1.5){ float g = texture(uFoliage, vec2(0.0625,0.0625)).r; O = vec4(g,g,g,1.0); return; }   // DIAGNOSE: feste Atlasstelle Kachel 0
  vec3 col = foliageLight(vPos, nrm, albedo, 1.0);
  // opak, solange keine Ueberblendung laeuft (sonst wirkt die Krone hohl:
  // jede Karte haette Alpha < 1). Echtes Alpha nur waehrend der Blende.
  // Aufnahme-Pass: die Kachel braucht ECHTES Alpha. Mit vFade = 1 (im
  // Aufnahme-Pass erzwungen) stand hier Alpha 1 fuer jedes gezeichnete Fragment
  // -> die Kachel hatte harte Kanten (gemessen: 211 161 Texel Alpha 255, 837 282
  // Alpha 0, nur 133 dazwischen). Ferne Billboards aliasen damit.
  if(uImpostor > 0.5){ O = vec4(col, cov); return; }
  // Die Ganzbaum-Kachel kriegt IMMER echtes Alpha: ihre Silhouette lebt von der
  // Textur-Alpha, ein Alpha von 1 macht bei gefilterter Kachel ein Rechteck.
  if(vCrown > 0.5){ O = vec4(col, cov); return; }
  // Alpha durchgehend = cov. Der fruehere Sprung `vFade >= 0.999 ? 1.0 : cov`
  // machte beim Start der Blende aus einem Texel mit a = 0,5 schlagartig alpha
  // 0,4995: die weiche Kartekante wurde instant durchsichtig, die Karte wirkte
  // einen Texel kleiner (Nutzerbefund " sprunghaft ein bisschen kleiner, der
  // Texeluebergang wandert um ein Texel"). Eine Blende darf keine Kantenregel
  // wechseln - cov ist bei vFade = 1 dasselbe wie vorher a.
  // Composite-Phase 2: das Gewicht kommt in den Alpha-Kanal, der
  // Composite-Pass mischt damit (comp.frag). Sonst sind Karten OPAK.
  O = vec4(col, uCompPhase > 1.5 ? clamp(vCompW, 0.0, 1.0) : 1.0);
}
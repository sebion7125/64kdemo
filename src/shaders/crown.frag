#version 330 core
precision highp float;
in vec2 vUV;
in vec3 vPos;
in float vSeed;
in float vFade;
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
uniform float uCrownMask;   // DEBUG: Kronen-Quads als reines Rot -> exakte Maske
out vec4 O;
const vec3 SUN = normalize(vec3(0.55, 0.38, -0.62));
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
float fbm(vec2 p){
  float s = 0.0, a = 0.5;
  for(int i = 0; i < 4; i++){
    s += a * vnoise(p);
    p = p * 2.03 + vec2(17.1, 9.2);
    a *= 0.5;
  }
  return s;
}
vec3 skyColor(vec3 rd){
  float t = clamp(rd.y, 0.0, 1.0);
  vec3 col = mix(vec3(0.67, 0.78, 0.96), vec3(0.22, 0.42, 0.82), pow(t, 0.55));
  float sd = max(dot(rd, SUN), 0.0);
  col += vec3(1.0, 0.85, 0.6) * (pow(sd, 900.0) * 10.0 + pow(sd, 80.0) * 0.3 + pow(sd, 8.0) * 0.12);
  if(rd.y > 0.03){
    vec2 cuv = rd.xz / (rd.y + 0.12) * 0.4;
    float cl = smoothstep(0.48, 0.72, fbm(cuv));
    vec3 cc = mix(vec3(1.02, 1.00, 0.98), vec3(0.85, 0.87, 0.95), fbm(cuv * 2.7 + 3.1));
    cc *= 0.55 + 0.45 * pow(sd, 3.0);
    col = mix(col, cc, cl * 0.8 * smoothstep(0.03, 0.18, rd.y));
  }
  return col;
}

// ---------- Schattenwurf (Shadow-Map, eine orthogliche Licht-Kaskade) ----------
uniform sampler2D uShadow;
uniform mat4  uLightMVP;
uniform float uShadowOn;
uniform float uShadowPx;      // 1 / Aufloesung
uniform float uShadowBias;    // Bias in Depth-Einheiten (CPU: 1,5 Texel in m umgerechnet)
// Rueckgabe: 1 = in Sonne, 0 = voll im Schatten.
// PCF: gefiltert werden die VERGLEICHSERGEBNISSE, nicht die Tiefen. GPU Gems 1
// Ch. 12 (Fernando/Everitt): "the comparison is performed at the four
// neighbouring shadow map samples, and the results are bilinearly filtered".
// Der alte Code mittelte neun DEPTH-Werte und verglich einmal -> hart, verschmiert
// und systematisch zu wenig Schatten.
float shadowAt(vec3 wp, vec3 n){
  if(uShadowOn < 0.5) return 1.0;
  if(uShadowOn > 1.5 && uShadowOn < 2.5) return 0.0;   // DIAGNOSE: alles beschattet
  vec3 lp = (uLightMVP * vec4(wp + n * 0.04, 1.0)).xyz;
  float z = lp.z * 0.5 + 0.5;                 // NDC [-1,1] -> Depth [0,1]
  vec2 uv = lp.xy * 0.5 + 0.5;
  if(z >= 1.0 || uv.x < 0.001 || uv.x > 0.999 || uv.y < 0.001 || uv.y > 0.999) return 1.0;
  float lit = 0.0;
  for(int y = -1; y <= 1; y++)
  for(int x = -1; x <= 1; x++)
    lit += (z - uShadowBias <= textureLod(uShadow, uv + vec2(float(x), float(y)) * uShadowPx, 0.0).r) ? 1.0 : 0.0;
  lit *= 0.1111111;
  // Am Rand der Licht-Frustum weich ausblenden: die Karte deckt 45 m um die
  // Kamera ab, ohne diesen Rand ploppen Schatten und ihre Luecken an der Kante.
  float edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
  return mix(1.0, lit, smoothstep(0.0, 0.07, edge));
}

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
    float clod = min(lod, 3.0);
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
  if(uDepthOnly > 0.5){ if(vFade < 0.5 || a < 0.5) discard; }
  else if(cov < 0.02) discard;
  // Masken-Diagnose erst nach dem Discard: sonst zaehlt sie auch die leeren
  // areas der Kachel und die Farbmessung mittelt den Hintergrund mit.
  if(uCrownMask > 0.5 && uCrownMask < 1.5){ O = vec4(1.0, 0.0, 0.0, 1.0); return; }
  // Beleuchtung wie im Blatt-Shader, nicht als Kugel: die Karte steht fuer einen
  // Buschel duenner Blaetter, also dieselbe Beleuchtung (beidseitig diffus,
  // Transluzenz, warmes Ambient). Vorher eigene Kugel-Schattierung mit 0,45
  // Diffus-Anteil -> Karten deutlich heller/grauer als das Nah-LOD (gemessen am
  // selben Baum: 60 m Karten (128,148,116) gegen 16 m Einzelblaetter (59,99,39)).
  vec3 nrm = normalize(vNrm + (vRight * vUV.x + vUp * vUV.y) * 0.55);
  float sh = shadowAt(vPos, nrm);
  float dif = abs(dot(nrm, SUN)) * mix(0.18, 1.0, sh) * 0.7 + 0.3;
  float thru = pow(max(dot(-nrm, SUN), 0.0), 1.5) * 0.8 * mix(0.2, 1.0, sh);
  vec3 amb = vec3(0.18, 0.32, 0.16) * (0.5 + 0.5 * max(nrm.y, 0.0)) * (0.62 + 0.38 * sh);
  // Die Kachel ist ein farbneutraler BLATTBUSCHEL (mehrere unserer Blaetter an
  // kurzen Stielen) — dieselbe Silhouette wie das Nah-LOD. Die Farbe kommt pro
  // Baum aus der Instanz, vorher steckte eine zufaellige Fremdfarbe in der Textur.
  vec3 albedo = tx.rgb * vCol;
  if(uCrownMask > 1.5){ O = vec4(albedo, 1.0); return; }   // DIAGNOSE: nur Albedo
  if(uCrownMask > 2.5){   // DIAGNOSE: R = Sonne, G = Ambient, B = Transluzenz
    O = vec4(dif, amb.g, thru, 1.0);
    return;
  }
  vec3 col = albedo * (vec3(1.0, 0.95, 0.80) * dif + amb + vec3(0.8, 1.0, 0.5) * thru);
  float s = length(vPos - uCamPos) - 60.0;   // Dunst wie fogAmt() in fragment.glsl
  float fog = s <= 0.0 ? 0.0 : 1.0 - exp(-s * s * 0.000012);
  col = mix(col, skyColor(normalize(uCamPos - vPos)), fog);
  // Tonemapping + Gamma (identisch zu fragment.glsl)
  col = 1.0 - exp(-col * 1.25);
  col = pow(col, vec3(0.4545));
  // opak, solange keine Ueberblendung laeuft (sonst wirkt die Krone hohl:
  // jede Karte haette Alpha < 1). Echtes Alpha nur waehrend der Blende.
  // Aufnahme-Pass: die Kachel braucht ECHTES Alpha. Mit vFade = 1 (im
  // Aufnahme-Pass erzwungen) stand hier Alpha 1 fuer jedes gezeichnete Fragment
  // -> die Kachel hatte harte Kanten (gemessen: 211 161 Texel Alpha 255, 837 282
  // Alpha 0, nur 133 dazwischen). Ferne Billboards aliasen damit.
  if(uImpostor > 0.5){ O = vec4(col, cov); return; }
  O = vec4(col, vFade >= 0.999 ? 1.0 : cov);
}
#version 330 core
precision highp float;
in vec3 vNrm;
in vec3 vPos;
in vec4 vCol;
in vec2 vUV;      // u = 0..1 einmal um das Rohr, v = Bogenlaenge in Metern
// Ueberblendung beim LOD-Wechsel (Runde 27): der alte Satz Geometrie wird
// waehrend der Ueberblendzeit weggeblendet, damit ein Baum nicht schlagartig
// die Detailstufe wechselt. Aufloesung unabhaengig von der Draw-Reihenfolge:
// pro Weltwuerfel gewuerfelt, wie das Laub es schon fuer den Blatt-/Karten-
// Uebergang tut.
uniform float uTreeFade = 1.0;
uniform vec3 uCamPos;
uniform float uTreeMask;   // DEBUG (F3): Baum-Mesh reines Rot -> exakte Maske,
                           // um Rinden-Kennzahlen nur auf Baum-Pixel zu messen
out vec4 O;
const vec3 SUN = normalize(vec3(0.55, 0.38, -0.62));
float hash12(vec2 p){
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float hash13(vec3 p){
  p = fract(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return fract((p.x + p.y) * p.z);
}
float n3(vec3 p){
  vec3 i = floor(p), f = fract(p);
  vec3 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(mix(hash13(i),                    hash13(i + vec3(1,0,0)), u.x),
                 mix(hash13(i + vec3(0,1,0)),      hash13(i + vec3(1,1,0)), u.x), u.y),
             mix(mix(hash13(i + vec3(0,0,1)),      hash13(i + vec3(1,0,1)), u.x),
                 mix(hash13(i + vec3(0,1,1)),      hash13(i + vec3(1,1,1)), u.x), u.y), u.z);
}
float vnoise(vec2 p){
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i),           hash12(i + vec2(1,0)), u.x),
             mix(hash12(i + vec2(0,1)), hash12(i + vec2(1,1)), u.x), u.y);
}
float fbm(vec2 p){
  float s = 0.0, a = 0.5;
  for(int i = 0; i < 3; i++){
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
// ---------- Rinde ----------
// Vorher: atan(vPos.x, vPos.z) (Welt-Winkel um den URSPRUNG — auf einem duennen
// Stamm ueberall gleich -> flaechige Farbe) und danach atan(n.z, n.x). Beides sind
// Naeherungen fuer SENKRECHTE Flaechen; an waagerechten Aesten zeigt die Normale
// aber nach oben, der Azimut wird zufaellig -> Rinde bricht am Abzweig weg.
// Neu: der Baum-Vertex traegt echte Rohr-Oberflaechenkoordinaten (u = 0..1 um den
// Ring, v = Bogenlaenge in m). Abgetastet wird auf einem Zylinder im Noiseraum,
// also nahtlos um den Umfang und auf jedem Rohr korrekt.
// circF = Noiszellen pro Umlauf: damit skaliert die Rindenbreite automatisch mit
// dem Rohrdurchmesser (duenne Aeste bekommen feinere Rinde, wie in der Natur).
float barkH(float u, float v, float circF, float lenF){
  float au = u * 6.2831853;
  float rr = circF / 6.2831853;
  vec3 q  = vec3(cos(au) * rr, sin(au) * rr, v * lenF);
  float a = n3(q) * 0.50 + n3(q * 2.3 + 11.7) * 0.32 + n3(q * 5.1 + 3.3) * 0.18;
  vec3 qr = vec3(cos(au) * rr * 1.6, sin(au) * rr * 1.6, v * lenF * 0.55);
  float c = 1.0 - abs(n3(qr) * 2.0 - 1.0);      // ridged -> tiefe Risse
  return a * 0.75 + c * c * 0.45;
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
  if(uTreeMask > 0.5){ O = vec4(1.0, 0.0, 0.0, 1.0); return; }
  // Ueberblendung beim LOD-Wechsel: der alte Satz loest sich pro Weltwuerfel auf
  // (uTreeFade 1 -> nichts wird verworfen, identisch zu vorher). Nah am Betrachter
  // bleibt das Geruest hart — ein loesender Stamm sieht aus wie ein kaputter
  // Stamm, und ferne Gerueste sind subpixelig, dort faellt der Wuerfelentscheid
  // nicht auf. Grenzwert 30 m: unterhalb davon wechselt in der Praxis nie die
  // Detailstufe (tier 3 reicht bis 10x Wuchshoehe ~ 60..90 m, der Sprung 3->2
  // aendert nur die Segmentzahl, also ohnehin fast nichts am Bild).
  if(uTreeFade < 1.0 && length(vPos - uCamPos) > 30.0 &&
     hash12(floor(vPos.xz * 3.0) + vec2(floor(vPos.y * 3.0) * 17.0)) > uTreeFade) discard;
  vec3 n = normalize(vNrm);
  vec3 rd = normalize(uCamPos - vPos);
  if(dot(n, rd) < 0.0) n = -n;
  float u = vUV.x, v = vUV.y;
  // vCol.a traegt den sway-Wert (0,05 Stammfuss .. 0,30 Stammkopf .. 1,0 Astspitze).
  // Damit laesst sich Stamm von Ast trennen und "am Fuss" ohne Extra-Pufferfeld.
  float relH = vCol.a;
  float circF = 26.0, lenF = 6.0;
  float h  = barkH(u, v, circF, lenF);
  float hu = barkH(u + 0.012, v, circF, lenF);
  float hv = barkH(u, v + 0.03, circF, lenF);
  // Duenne Aeste werden glaetter und gruenlicher, aber NICHT texturlos: Rinde
  // bleibt auf jedem Rohr, nur feiner und flaecher.
  float onTrunk = 1.0 - smoothstep(0.30, 0.75, relH);
  float bumpAmt = 0.055 + 0.075 * onTrunk;
  vec3 t1 = normalize(cross(vec3(0.0, 1.0, 0.0), n) + vec3(1e-5));
  vec3 t2 = normalize(cross(n, t1));
  vec3 nb = normalize(n - ((hu - h) / 0.012) * bumpAmt * t1
                        - ((hv - h) / 0.03)  * bumpAmt * t2);
  float coarse = fbm(vec2(u * 3.0, v * 0.65));      // Verwitterungs-Flecken
  vec3 hell  = mix(vec3(0.36, 0.27, 0.17), vec3(0.30, 0.26, 0.20), coarse);
  vec3 dunkel= vec3(0.10, 0.075, 0.05);
  vec3 albedo = mix(dunkel, hell, clamp(h * 1.25, 0.0, 1.0));
  // Moos/Flechten am Stammfuss
  float moss = smoothstep(0.58, 0.86, fbm(vec2(u * 6.0 + 3.0, v * 1.3)))
             * (1.0 - smoothstep(0.05, 0.20, relH));
  albedo = mix(albedo, vec3(0.18, 0.28, 0.12), moss * 0.55);
  // duenne Aeste: eigener Ton, aber mit derselben Rindenstruktur
  albedo = mix(albedo, vCol.rgb * (0.85 + 0.3 * coarse) * (0.55 + 0.9 * h), 1.0 - onTrunk);
  float sh = shadowAt(vPos, nb);
  float dif = max(dot(nb, SUN), 0.0) * 0.85 * sh + 0.15;
  vec3 amb = vec3(0.40, 0.50, 0.68) * (0.45 + 0.55 * max(nb.y, 0.0)) * (0.62 + 0.38 * sh);
  vec3 col = albedo * (vec3(1.0, 0.92, 0.78) * dif + amb);
  float s = length(vPos - uCamPos) - 60.0;   // Dunst wie fogAmt() in fragment.glsl
  float fog = s <= 0.0 ? 0.0 : 1.0 - exp(-s * s * 0.000012);
  col = mix(col, skyColor(rd), fog);
  // Tonemapping + Gamma (identisch zu fragment.glsl, da Mesh jetzt direkt
  // ueber die schon tonemapte Szene geblendet/geschrieben wird)
  col = 1.0 - exp(-col * 1.25);
  col = pow(col, vec3(0.4545));
  O = vec4(col, 1.0);
}
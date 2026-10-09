// GEMEINSAMER LAUB-SHADERWEG (Nutzerentscheid 2026-10-09, Punkt 2).
// leaf.frag (Einzelblaetter) und crown.frag (Cluster-Quadate) binden diese
// Datei ein. Vorher standen skyColor(), shadowAt() und die Beleuchtung je
// zweimal im Quelltext: 600 + 899 Byte doppelt, und zwei Stellen, die man bei
// einer Aenderung leicht vergisst. Die Farbe muss ueber alle Stufen identisch
// sein, weil die Stufen ueberblenden - jede Verdopplung ist eine Fehlerquelle
// (gemessen in Runde 8: Karten ohne Tonemapping; Runde 13: Karten heller).
//
// Setzt in beiden Fragment-Shadern voraus:
//   uniform vec3 uCamPos; uniform sampler2D uShadow; uniform mat4 uLightMVP;
//   uniform float uShadowOn, uShadowPx, uShadowBias; const vec3 SUN;
float hash12(vec2 p){
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}float vnoise(vec2 p){
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i),           hash12(i + vec2(1,0)), u.x),
             mix(hash12(i + vec2(0,1)), hash12(i + vec2(1,1)), u.x), u.y);
}float fbm(vec2 p){
  float s = 0.0, a = 0.5;
  for(int i = 0; i < 4; i++){
    s += a * vnoise(p);
    p = p * 2.03 + vec2(17.1, 9.2);
    a *= 0.5;
  }
  return s;
}vec3 skyColor(vec3 rd){
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
}float shadowAt(vec3 wp, vec3 n){
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
// Laub ist duenn: beidseitig diffus + Transluzenz + gruenliches Ambient, danach
// Dunst, Tonemapping und Gamma (identisch zu fragment.glsl). kmul multipliziert
// das beleuchtete Ergebnis VOR dem Dunst (Einzelblaetter variieren damit um
// +/-15 %, Karten mit 1,0).
vec3 foliageLight(vec3 p, vec3 n, vec3 albedo, float kmul){
  n = normalize(n);
  vec3 rd = normalize(uCamPos - p);
  float sh = shadowAt(p, n);
  float dif = abs(dot(n, SUN)) * mix(0.18, 1.0, sh);
  float thru = pow(max(dot(-rd, SUN), 0.0), 1.5) * 0.8 * mix(0.2, 1.0, sh);
  vec3 amb = vec3(0.18, 0.32, 0.16) * (0.5 + 0.5 * max(n.y, 0.0)) * (0.62 + 0.38 * sh);
  vec3 col = albedo * (vec3(1.0, 0.95, 0.80) * (dif * 0.7 + 0.3) + amb + vec3(0.8, 1.0, 0.5) * thru);
  col *= kmul;
  float s = length(p - uCamPos) - 60.0;
  float fog = s <= 0.0 ? 0.0 : 1.0 - exp(-s * s * 0.000012);
  col = mix(col, skyColor(rd), fog);
  col = 1.0 - exp(-col * 1.25);
  return pow(col, vec3(0.4545));
}

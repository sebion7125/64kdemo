#version 330 core
precision highp float;
in vec3 vNrm;
in vec3 vPos;
in vec3 vCol;
in float vT;
in float vW;
in float vAsp;
uniform vec3 uCamPos;
uniform float uLeafMask;   // DEBUG: Blatt als Maske (reines Rot)
uniform float uAlbedoOnly; // Atlas-Kapture: nur Albedo + Abdeckung (keine Beleuchtung)
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
  return lit * 0.1111111;
}

void main(){
  // Masken-Diagnose ERST nach dem Discard der Blattsilhouette: sonst zaehlt die
  // Maske auch die leeren Quad-Ecken und die Farbmessung mittelt Hintergrund mit.
  // ---- Blattsilhouette (Vorlage: Sapling — Blätter sind in die Länge gestreckt,
  // niemals quadratisch; die Silhouette kommt aus der Textur/dem Shader, nicht
  // aus der Geometrie). vT = -0.5..0.5 entlang, vW = -0.5..0.5 quer.
  float tt = vT + 0.5;                                  // 0 Stiel .. 1 Spitze
  float hw = 0.5 * vAsp * pow(sin(3.14159 * pow(tt, 0.55)), 0.70);  // breiteste Stelle
  hw *= 1.0 - 0.07 * step(0.5, fract(tt * 17.0));       // feiner Sägeblattrand
  hw = max(hw, 1e-4);                                   // bei tt->0 wird hw klein
  float e = abs(vW) / hw;
  if(e > 1.0) discard;
  if(uLeafMask > 0.5 && uLeafMask < 1.5){ O = vec4(1.0, 0.0, 0.0, 1.0); return; }
  float midrib = 1.0 - smoothstep(0.0, 0.055, abs(vW)); // Mittelrippe
  float vein   = 1.0 - smoothstep(0.0, 0.05,
                    abs(fract((tt + abs(vW) * 0.9) * 5.0) - 0.5) * 0.34 - abs(vW) * 0.02);
  vec3 n = normalize(vNrm);
  vec3 rd = normalize(uCamPos - vPos);
  // Blaetter sind duenn: beidseitig beleuchtet (kein "blaues" Ambient mehr)
  float sh = shadowAt(vPos, n);
  float dif = abs(dot(n, SUN)) * mix(0.18, 1.0, sh);
  // Transluzenz: Licht durchs Blatt (gruenstichig, dominant)
  float thru = pow(max(dot(-rd, SUN), 0.0), 1.5) * 0.8 * mix(0.2, 1.0, sh);
  // weiches, warmes Ambient (weniger blau)
  vec3 amb = vec3(0.35, 0.45, 0.40) * (0.5 + 0.5 * max(n.y, 0.0)) * (0.62 + 0.38 * sh);
  float pat = vnoise(vPos.xz * 16.0 + vPos.y * 9.0);
  vec3 albedo = vCol * (0.72 + 0.55 * pat);
  albedo *= 1.0 + midrib * 0.35 - vein * 0.10;          // Rippen heller, Adern dunkler
  albedo *= 1.0 - 0.18 * smoothstep(0.75, 1.0, e);      // Rand etwas dunkler
  if(uLeafMask > 1.5){ O = vec4(albedo, 1.0); return; } // DIAGNOSE: nur Albedo
  if(uAlbedoOnly > 0.5){ O = vec4(albedo, 1.0); return; }
  vec3 col = albedo * (vec3(1.0, 0.95, 0.80) * (dif * 0.7 + 0.3) + amb + vec3(0.8, 1.0, 0.5) * thru);
  col *= 0.85 + 0.15 * vT;
  float s = length(vPos - uCamPos) - 60.0;   // Dunst wie fogAmt() in fragment.glsl
  float fog = s <= 0.0 ? 0.0 : 1.0 - exp(-s * s * 0.000012);
  col = mix(col, skyColor(rd), fog);
  O = vec4(col, 1.0);
}
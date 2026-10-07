#version 330 core
precision highp float;
in vec2 vUV;
in vec3 vPos;
in float vSeed;
in float vFade;
in vec2 vTile;      // Atlas-Kachel (Spalte, Zeile)
in vec3 vNrm;       // Billboard-Normale
in vec3 vRight, vUp;
uniform vec3 uCamPos;
uniform sampler2D uFoliage;   // Atlas: 8x8 Kacheln echter Blattwerk-Aufnahmen
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
  if(tx.a < 0.5 * vFade) discard;
  // Beleuchtung: die Kachel enthaelt Albedo, nicht Licht. Eine Kugel-Andeutung
  // ueber die Quad-Ebene gibt der Krone Volumen (ferne Kronen sind zu klein fuer
  // echte Normale aus der Textur).
  vec3 nrm = normalize(vNrm + (vRight * vUV.x + vUp * vUV.y) * 0.55);
  float dif = 0.45 + 0.55 * max(dot(nrm, SUN), 0.0);
  float back = pow(max(dot(-nrm, SUN), 0.0), 2.0) * 0.5;   // Gegenlicht-Schimmer
  vec3 albedo = tx.rgb;
  vec3 col = albedo * (vec3(1.0, 0.95, 0.80) * (dif + back)
                      + vec3(0.35, 0.45, 0.40) * 0.55);
  float s = length(vPos - uCamPos) - 60.0;   // Dunst wie fogAmt() in fragment.glsl
  float fog = s <= 0.0 ? 0.0 : 1.0 - exp(-s * s * 0.000012);
  col = mix(col, skyColor(normalize(uCamPos - vPos)), fog);
  // Tonemapping + Gamma (identisch zu fragment.glsl)
  col = 1.0 - exp(-col * 1.25);
  col = pow(col, vec3(0.4545));
  O = vec4(col, 1.0);
}
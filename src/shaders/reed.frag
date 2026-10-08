#version 330 core
precision highp float;
// Schilf-Pass (TODO 26): Farben und Beleuchtung sind aus grass.frag uebernommen
// (Spezies-4-Zweig), damit der Umstieg nichts an der Optik aendert.
in vec3 vNrm;
in vec3 vPos;
in vec3 vCol;
in float vT;
in float vFade;
in float vRole;   // 1 = Culm, 2 = Blatt
in float vSide;
in vec3 vR;
in float vHead;   // Kolben-Anteil
in float vVar;
uniform vec3 uCamPos;
uniform float uAlbedoOnly;
uniform float uDbgCat;   // Arten-Modus: Schilf in derselben Farbe wie grass.frag
out vec4 O;
const vec3 SUN = normalize(vec3(0.55, 0.38, -0.62));
const float WATER_Y = 0.25;
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
  return col;
}
uniform sampler2D uShadow;
uniform mat4  uLightMVP;
uniform float uShadowOn;
uniform float uShadowPx;
uniform float uShadowBias;
float shadowAt(vec3 wp, vec3 n){
  if(uShadowOn < 0.5) return 1.0;
  if(uShadowOn > 1.5 && uShadowOn < 2.5) return 0.0;
  vec3 lp = (uLightMVP * vec4(wp + n * 0.04, 1.0)).xyz;
  float z = lp.z * 0.5 + 0.5;
  vec2 uv = lp.xy * 0.5 + 0.5;
  if(z >= 1.0 || uv.x < 0.001 || uv.x > 0.999 || uv.y < 0.001 || uv.y > 0.999) return 1.0;
  float lit = 0.0;
  for(int y = -1; y <= 1; y++)
  for(int x = -1; x <= 1; x++)
    lit += (z - uShadowBias <= textureLod(uShadow, uv + vec2(float(x), float(y)) * uShadowPx, 0.0).r) ? 1.0 : 0.0;
  return lit * 0.1111111;
}
void main(){
  vec3 n = normalize(vNrm);
  n = normalize(n + 2.0 * 0.6 * vSide * vR + 0.5 * vec3(0.0, 1.0, 0.0));
  vec3 rd = normalize(uCamPos - vPos);
  // Grundfarbe wie am Boden (Gras/Sand), dann die Schilffarbe darauf.
  float g = fbm(vPos.xz * 0.9);
  vec3 tGrass = mix(vec3(0.18, 0.46, 0.09), vec3(0.37, 0.66, 0.12), g) * vec3(0.87, 0.93, 0.89);
  float sandM = 1.0 - smoothstep(0.05, 0.40, abs(vPos.y - WATER_Y));
  vec3 sand = mix(vec3(0.62, 0.55, 0.38), vec3(0.72, 0.66, 0.48), fbm(vPos.xz * 1.5));
  vec3 base = mix(tGrass, sand, sandM);
  vec3 tip  = vec3(0.34, 0.64, 0.11);
  base = mix(base, vec3(0.06, 0.23, 0.06), 0.75);      // Schilfguertel
  if(vRole > 1.5){                                     // Blatt: graugruen
    base = mix(base, vec3(0.04, 0.20, 0.03), 0.85);
    tip  = mix(tip,  vec3(0.07, 0.26, 0.05), 0.88);
  } else {                                             // Culm: gruen-gelb, oben braun
    base = mix(base, vec3(0.07, 0.24, 0.06), 0.75);
    tip  = mix(tip,  vec3(0.44, 0.42, 0.30), smoothstep(0.45, 0.66, vT));
    tip  = mix(tip,  vec3(0.46, 0.40, 0.30), smoothstep(0.66, 0.86, vT));
  }
  // Kolben: buschiger Stand — ueber eine quer-modulierte Maske loechrig, damit
  // er nicht als geschlossene Keule steht.
  if(vHead > 0.5){
    float u = vSide / 0.34;
    float e = abs(u) * (1.0 + 0.35 * sin(vPos.y * 90.0 + vVar * 20.0));
    if(e > 0.85) discard;
  }
  if(uDbgCat > 1.5){ base = vec3(0.95, 0.45, 0.05); tip = base; }   // = Schilffarbe in grass.frag
  vec3 albedo = mix(base, tip, vT * vT) * (0.75 + 0.5 * vCol.x);
  if(uAlbedoOnly > 0.5){ O = vec4(albedo, 1.0); return; }
  float sh = shadowAt(vPos, n);
  float dif = max(dot(n, SUN), 0.0) * 0.85 * sh + 0.15;
  vec3 amb = vec3(0.40, 0.50, 0.68) * (0.45 + 0.55 * max(n.y, 0.0)) * (0.62 + 0.38 * sh);
  vec3 col = albedo * (vec3(1.0, 0.92, 0.78) * dif + amb);
  float tl = pow(max(dot(-rd, SUN), 0.0), 3.0) * (0.3 + 0.7 * vT) * (0.25 + 0.75 * sh);
  col += albedo * tl * 0.55;
  vec3 hv = normalize(rd + SUN);
  float spec = pow(max(dot(n, hv), 0.0), 24.0) * (0.15 + 0.85 * vT * vT);
  col += vec3(1.0, 0.98, 0.9) * spec * 0.12;
  float s = length(vPos - uCamPos) - 60.0;
  float fog = s <= 0.0 ? 0.0 : 1.0 - exp(-s * s * 0.000012);
  col = mix(col, skyColor(rd), fog);
  col = 1.0 - exp(-col * 1.25);
  col = pow(col, vec3(0.4545));
  O = vec4(col, vFade);
}

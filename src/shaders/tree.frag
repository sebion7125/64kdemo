#version 330 core
precision highp float;
in vec3 vNrm;
in vec3 vPos;
in vec4 vCol;
uniform vec3 uCamPos;
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
void main(){
  vec3 n = normalize(vNrm);
  vec3 rd = normalize(uCamPos - vPos);
  if(dot(n, rd) < 0.0) n = -n;
  // Rinde: vertikale Riefen
  float ang = atan(vPos.x, vPos.z);
  float bark = vnoise(vec2(ang * 2.5, vPos.y * 1.2)) * 0.6
             + vnoise(vec2(ang * 6.0, vPos.y * 3.0)) * 0.4;
  vec3 albedo = vCol.rgb * (0.72 + 0.55 * bark);
  float dif = max(dot(n, SUN), 0.0) * 0.85 + 0.15;
  vec3 amb = vec3(0.40, 0.50, 0.68) * (0.45 + 0.55 * max(n.y, 0.0));
  vec3 col = albedo * (vec3(1.0, 0.92, 0.78) * dif + amb);
  float t = length(vPos - uCamPos);
  float fog = 1.0 - exp(-t * t * 0.000015);
  col = mix(col, skyColor(rd), fog);
  // Tonemapping + Gamma (identisch zu fragment.glsl, da Mesh jetzt direkt
  // ueber die schon tonemapte Szene geblendet/geschrieben wird)
  col = 1.0 - exp(-col * 1.25);
  col = pow(col, vec3(0.4545));
  O = vec4(col, 1.0);
}
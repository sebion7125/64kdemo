#version 330 core
precision highp float;
out vec4 O;
uniform vec2  uRes;
uniform float uTime;
uniform vec3  uCamPos;
uniform vec3  uCamTgt;

const vec3 SUN = normalize(vec3(0.55, 0.38, -0.62));
const float WATER_Y = 0.25;   // Wasserstand (mit main.c abgestimmt)
const float F0 = 0.1, N0 = 300.0;  // muss zu build_mvp() in main.c passen
// Projektions-Z-Konstanten (zu build_mvp): NDC.z = -PA + PB/tproj
const float PA = (F0 + N0) / (F0 - N0);
const float PB = 2.0 * F0 * N0 / (F0 - N0);

// ---------------- noise ----------------
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
float rfbm(vec2 p){
  float s = 0.0, a = 0.5;
  for(int i = 0; i < 4; i++){
    s += a * (1.0 - abs(2.0 * vnoise(p) - 1.0));
    p = p * 2.11 + vec2(9.7, 3.1);
    a *= 0.5;
  }
  return s;
}

// ---------------- terrain ----------------
float terrainH(vec2 p){
  float base  = fbm(p * 0.010);
  float mask  = smoothstep(0.38, 0.72, base);
  float r     = rfbm(p * 0.017);
  float mts   = pow(r, 2.5) * 32.0 * mask;
  float hills = (fbm(p * 0.045) - 0.5) * 5.0;
  float det   = (fbm(p * 0.40)  - 0.5) * 0.3;
  return hills + mts + det;
}
// Wasserflaeche: chaotische Oberflaechenwellen (Wellenlaenge ~1.5-4m, verschiedene Richtungen)
float waterH(vec2 p){
  float t = uTime;
  float w = 0.0;
  // mittlere Wellen (Wellenlaenge ~3-4m)
  w += 0.016 * sin(dot(p, vec2( 1.30,  0.90)) + t * 1.0);
  w += 0.012 * sin(dot(p, vec2(-1.70,  1.20)) + t * 1.3 + 1.7);
  w += 0.010 * sin(dot(p, vec2( 1.10, -1.90)) + t * 1.6 + 2.9);
  // feine Rippel (Wellenlaenge ~1.4-2m)
  w += 0.007 * sin(dot(p, vec2( 3.20,  2.10)) + t * 2.2 + 0.5);
  w += 0.005 * sin(dot(p, vec2(-2.50, -3.80)) + t * 2.6 + 3.7);
  w += 0.004 * sin(dot(p, vec2( 4.10,  1.00)) + t * 3.0 + 1.1);
  w += 0.003 * sin(dot(p, vec2(-1.50,  2.80)) + t * 3.4 + 2.3);
  return WATER_Y + w;
}

// ---------------- scene (nur Terrain + Wasser; Baeume/Gras = Mesh) ----------------
float sceneDist(vec3 p, out float matId){
  float h  = terrainH(p.xz);
  float dt = (p.y - h) * 0.85;
  float dw = (p.y - waterH(p.xz)) * 0.8;
  matId = (dt < dw) ? 1.0 : 3.0;
  return min(dt, dw);
}
float march(vec3 ro, vec3 rd, out float matId){
  float t = 0.02;
  matId = 0.0;
  for(int i = 0; i < 128; i++){
    vec3 p = ro + rd * t;
    float d = sceneDist(p, matId);
    if(d < 0.004 + t * 0.0025) return t;
    t += d * 0.9;
    if(t > 250.0) return -1.0;
  }
  return -1.0;
}
// Reflexions-March: nur Terrain (Baeume sind Mesh und nicht im SDF)
float marchTerrain(vec3 ro, vec3 rd){
  float t = 0.05;
  for(int i = 0; i < 96; i++){
    float d = (ro.y + rd.y * t - terrainH(ro.xz + rd.xz * t)) * 0.85;
    if(d < 0.004 + t * 0.0025) return t;
    t += d * 0.9;
    if(t > 200.0) return -1.0;
  }
  return -1.0;
}
vec3 calcNormal(vec3 p, float matId){
  const float e = 0.05;
  if(matId > 2.5){
    // Wasser: analytische Normal aus der Wellenflaeche (fein)
    float e2 = 0.05;
    float dx = waterH(p.xz + vec2(e2,0.0)) - waterH(p.xz - vec2(e2,0.0));
    float dz = waterH(p.xz + vec2(0.0,e2)) - waterH(p.xz - vec2(0.0,e2));
    return normalize(vec3(-dx, 2.0 * e2, -dz));
  }
  float m0, m1;
  return normalize(vec3(
    sceneDist(p + vec3(e,0,0), m0) - sceneDist(p - vec3(e,0,0), m1),
    sceneDist(p + vec3(0,e,0), m0) - sceneDist(p - vec3(0,e,0), m1),
    sceneDist(p + vec3(0,0,e), m0) - sceneDist(p - vec3(0,0,e), m1)));
}

// ---------------- sky ----------------
vec3 skyColor(vec3 rd){
  float t = clamp(rd.y, 0.0, 1.0);
  vec3 col = mix(vec3(0.72, 0.83, 0.95), vec3(0.22, 0.42, 0.82), pow(t, 0.55));
  float sd = max(dot(rd, SUN), 0.0);
  col += vec3(1.0, 0.85, 0.6) * (pow(sd, 900.0) * 10.0 + pow(sd, 80.0) * 0.3 + pow(sd, 8.0) * 0.12);
  if(rd.y > 0.03){
    vec2 cuv = rd.xz / (rd.y + 0.12) * 0.4 + vec2(uTime * 0.012, uTime * 0.005);
    float cl = smoothstep(0.48, 0.72, fbm(cuv));
    vec3 cc = mix(vec3(1.02, 1.00, 0.98), vec3(0.85, 0.87, 0.95), fbm(cuv * 2.7 + 3.1));
    cc *= 0.55 + 0.45 * pow(sd, 3.0);
    col = mix(col, cc, cl * 0.8 * smoothstep(0.03, 0.18, rd.y));
  }
  return col;
}

// ---------------- shading ----------------
vec3 terrainColor(vec3 p, vec3 n, float h, vec3 rd){
  float g = fbm(p.xz * 0.9);
  // Gras-Farbe an die Halme (grass.frag) angepasst: gleiche Beleuchtung, aber
  // die Terrain-Albedo war ~7-15% heller (gelblicher) als der Halm-Mittelwert
  // -> beim LOD-Fade in der Ferne war der Uebergang sichtbar. Scale (0.87,
  // 0.93, 0.89) = Halm-Mittelwert / Terrain-Mittelwert (siehe Albedo in
  // grass.frag: mix(base,tip,vT^2) * (0.75+0.5*sh), Mittel = 0.304/0.475/0.138).
  vec3 grass = mix(vec3(0.22, 0.42, 0.13), vec3(0.48, 0.60, 0.18), g) * vec3(0.87, 0.93, 0.89);
  // Blumen
  vec2 fc = floor(p.xz * 1.3);
  float fl = hash12(fc);
  if(fl > 0.983){
    vec2 off = (vec2(hash12(fc * 1.31), hash12(fc * 1.73)) - 0.5);
    float dd = length(fract(p.xz * 1.3) - 0.5 - off * 0.5);
    float m = smoothstep(0.10, 0.04, dd);
    vec3 fc2 = (fl > 0.991) ? vec3(0.95, 0.85, 0.25) : vec3(0.95, 0.95, 0.95);
    grass = mix(grass, fc2, m);
  }
  float slope = length(n.xz);
  float rockM = max(smoothstep(0.45, 0.75, slope), smoothstep(9.0, 15.0, p.y));
  vec3 rock = mix(vec3(0.42, 0.38, 0.34), vec3(0.55, 0.52, 0.48), fbm(p.xz * 0.6));
  vec3 col = mix(grass, rock, rockM);
  float snowM = smoothstep(17.0, 22.0, p.y) * (1.0 - smoothstep(0.35, 0.6, slope));
  col = mix(col, vec3(0.93, 0.95, 1.0), snowM);
  // Sand am Ufer (0.05..0.40m um die Wasserlinie)
  float sandM = (1.0 - smoothstep(0.05, 0.40, abs(h - WATER_Y))) * (1.0 - rockM);
  vec3 sand = mix(vec3(0.62, 0.55, 0.38), vec3(0.72, 0.66, 0.48), fbm(p.xz * 1.5));
  col = mix(col, sand, sandM);
  // Feines Gras-Detail (Top-Down-Sicht): kurze Strokes, wo die Flaechen
  // frontal von oben gesehen wird (dort sind die geometrischen Haelme kantig)
  {
    float face = -dot(rd, n);
    float onGrass = (1.0 - rockM) * (1.0 - sandM) * (1.0 - snowM);
    float st = smoothstep(0.55, 0.9, face) * onGrass;
    if(st > 0.004){
      vec2 gp = p.xz * 5.0;
      vec2 gi = floor(gp);
      float h1 = hash12(gi);
      float h2 = hash12(gi + 13.7);
      vec2 off = (vec2(h1, h2) - 0.5) * 0.6;
      vec2 sd = fract(gp) - 0.5 - off;
      float ang = h1 * 6.2831;
      vec2 dir = vec2(cos(ang), sin(ang));
      float m = smoothstep(0.05, 0.015, abs(dot(sd, vec2(-dir.y, dir.x))))
              * smoothstep(0.30, 0.05, abs(dot(sd, dir)));
      col = mix(col, col * mix(0.7, 1.45, h2), m * st * 0.8);
    }
  }
  return col;
}
vec3 waterColor(vec3 p, vec3 n, vec3 rd, float t){
  float depth = max(WATER_Y - terrainH(p.xz), 0.0);
  // Grundfarbe: Sand (shallow) -> tiefes Blau
  vec3 shallow = vec3(0.10, 0.42, 0.40);
  vec3 deep    = vec3(0.03, 0.12, 0.22);
  vec3 bot = mix(shallow, deep, clamp(depth * 0.18, 0.0, 1.0));
  // Reflexion: Himmel + Terrain (Baeume sind Mesh, nicht im SDF)
  vec3 rr = reflect(rd, n);
  rr = normalize(rr + vec3(0.0, 0.02, 0.0));
  vec3 refl = skyColor(rr);
  float tr = marchTerrain(p, rr);
  if(tr > 0.0){
    vec3 rp = p + rr * tr;
    vec3 rn = normalize(vec3(
      terrainH(rp.xz + vec2(0.05,0.0)) - terrainH(rp.xz - vec2(0.05,0.0)),
      0.1,
      terrainH(rp.xz + vec2(0.0,0.05)) - terrainH(rp.xz - vec2(0.0,0.05))));
    float rdiff = max(dot(rn, SUN), 0.0) * 0.85 + 0.15;
    vec3 ramb = vec3(0.40, 0.50, 0.68) * (0.45 + 0.55 * max(rn.y, 0.0));
    vec3 rcol = terrainColor(rp, rn, terrainH(rp.xz), -rr) * (vec3(1.0,0.92,0.78) * rdiff + ramb);
    float rfog = 1.0 - exp(-tr * tr * 0.00004);
    rcol = mix(rcol, skyColor(rr), rfog);
    refl = rcol;
  }
  float fres = 0.02 + 0.98 * pow(1.0 - max(dot(n, -rd), 0.0), 5.0);
  vec3 col = mix(bot, refl, clamp(fres * 1.6, 0.0, 1.0));
  // Sonnen-Glitzer
  float glint = pow(max(dot(reflect(-SUN, n), rd), 0.0), 180.0);
  col += vec3(1.0, 0.95, 0.8) * glint * 1.5;
  float dif = max(dot(n, SUN), 0.0) * 0.85 + 0.15;
  col *= 0.75 + 0.25 * dif;
  float fog = 1.0 - exp(-t * t * 0.00004);
  col = mix(col, skyColor(rd), fog);
  return col;
}
vec3 shade(vec3 p, vec3 n, vec3 rd, float t, float matId){
  vec3 albedo;
  if(matId < 1.5){
    albedo = terrainColor(p, n, terrainH(p.xz), rd);
  } else {
    return waterColor(p, n, rd, t);
  }
  float dif = max(dot(n, SUN), 0.0);
  dif = dif * 0.85 + 0.15;
  float ao = 1.0;
  {
    float oc = 0.0;
    for(int i = 1; i <= 4; i++){
      float hh = float(i) * 0.4;
      oc += max(0.0, terrainH(p.xz + n.xz * hh) - (p.y + n.y * hh)) * (1.2 - float(i) * 0.2);
    }
    ao = clamp(1.0 - oc * 0.35, 0.35, 1.0);
  }
  vec3 amb = vec3(0.40, 0.50, 0.68) * (0.45 + 0.55 * max(n.y, 0.0));
  vec3 col = albedo * (vec3(1.0, 0.92, 0.78) * dif * ao + amb);
  float fog = 1.0 - exp(-t * t * 0.00004);
  col = mix(col, skyColor(rd), fog);
  return col;
}

void main(){
  vec2 uv = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y;
  vec3 ro = uCamPos;
  vec3 fwd = normalize(uCamTgt - ro);
  vec3 right = normalize(cross(fwd, vec3(0,1,0)));
  vec3 up = cross(right, fwd);
  vec3 rd = normalize(fwd * 1.7 + right * uv.x + up * uv.y);

  float matId;
  float t = march(ro, rd, matId);
  vec3 col;
  if(t < 0.0){
    col = skyColor(rd);
  } else {
    vec3 p = ro + rd * t;
    vec3 n = calcNormal(p, matId);
    col = shade(p, n, rd, t, matId);
  }
  // Projektionstiefe (exakt zu build_mvp) fuer den Depth-Test des Mesh-Passes.
  // +0.2m Bias: auf der Oberflaeche liegende Mesh-Geometrie (Gras-Basis,
  // Stamm-Fuss) soll gegen das Raymarch-Terrain gewinnen (kein Z-Fighting).
  // 0.2m (statt 0.05m): llvmpipe rundet die Tiefe anders als NVIDIA, bei
  // 0.05m verliert das Gras in der Ferne den Depth-Test (nur am Rand sichtbar).
  if (t < 0.0) {
    gl_FragDepth = 1.0;   // Himmel = fern
  } else {
    float tproj = dot(rd, fwd) * t + 0.2;
    if (tproj < F0) tproj = F0;
    float ndc = -PA + PB / tproj;
    gl_FragDepth = (ndc + 1.0) * 0.5;
  }
  col = 1.0 - exp(-col * 1.25);
  col = pow(col, vec3(0.4545));
  O = vec4(col, 1.0);
}
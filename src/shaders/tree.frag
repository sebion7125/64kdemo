#version 330 core
precision highp float;
in vec3 vNrm;
in vec3 vPos;
in vec4 vCol;
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
// Die alte Version benutzte atan(vPos.x, vPos.z) — das ist der Welt-Winkel um den
// WELTURSPRUNG, nicht der Winkel um den Stamm. Ein duenner Stamm in 100 m
// Entfernung hat ueber seine ganze Oberflaeche praktisch denselben Welt-Winkel,
// deshalb war die "Rinde" eine flaechige Farbe ohne Struktur.
// Neu: Umfangs-Koordinate aus der NORMALEN (die Rinde zeigt nach aussen, also ist
// der Azimut-Winkel der Normalen genau die Umfangsposition am Stamm) plus Welt-Y
// als Hohen-Koordinate. Fuer senkrechte Staemme exakt, fuer Aeste naherungsweise.
float barkH(float u, float v){
  // u ist in RAD am Stammumfang (1 rad = ~0,2 m bei 20 cm Radius), v in Metern.
  // Echte Rindenriefen sind 1..3 cm breit und in die Laenge gestreckt -> die
  // Umfangs-Frequenz ist deutlich hoeher als die vertikale.
  float a = vnoise(vec2(u * 7.0,        v * 3.0 )) * 0.55
          + vnoise(vec2(u * 17.0 + 5.0, v * 8.0 )) * 0.30
          + vnoise(vec2(u * 37.0 + 1.0, v * 18.0)) * 0.15;
  // Ridged noise -> tiefe Risse (scharfe Taeler statt weicher Hugel)
  float c = 1.0 - abs(vnoise(vec2(u * 11.0 + 9.0, v * 4.0)) * 2.0 - 1.0);
  return a * 0.78 + c * c * 0.42;
}
void main(){
  if(uTreeMask > 0.5){ O = vec4(1.0, 0.0, 0.0, 1.0); return; }
  vec3 n = normalize(vNrm);
  vec3 rd = normalize(uCamPos - vPos);
  if(dot(n, rd) < 0.0) n = -n;
  float u = atan(n.z, n.x);        // Umfangsposition an der Rinde
  float v = vPos.y;                // Hoehe
  // vCol.a traegt den sway-Wert (0,05 Stammfuss .. 0,30 Stammkopf .. 1,0 Astspitze).
  // Damit laesst sich Stamm von Ast trennen und "am Fuss" ohne Extra-Pufferfeld.
  float relH = vCol.a;
  float h  = barkH(u, v);
  float hx = barkH(u + 0.02, v);
  float hy = barkH(u, v + 0.02);
  // Rinde nur am Stamm; duenne Aeste werden glaetter und gruenlicher
  float onTrunk = 1.0 - smoothstep(0.30, 0.55, relH);
  // Normalen storen (Bump) — macht die Riefen bei seitlichem Licht sichtbar
  vec3 t1 = normalize(cross(vec3(0.0, 1.0, 0.0), n) + vec3(1e-5));
  vec3 t2 = normalize(cross(n, t1));
  vec3 nb = normalize(n - ((hx - h) / 0.02) * 0.10 * onTrunk * t1
                        - ((hy - h) / 0.02) * 0.10 * onTrunk * t2);
  float coarse = fbm(vec2(u * 0.45, v * 0.65));      // Verwitterungs-Flecken
  vec3 hell  = mix(vec3(0.36, 0.27, 0.17), vec3(0.30, 0.26, 0.20), coarse);
  vec3 dunkel= vec3(0.10, 0.075, 0.05);
  vec3 albedo = mix(dunkel, hell, clamp(h * 1.25, 0.0, 1.0));
  // Moos/Flechten am Stammfuss (feucht, Schatten) und an Nord-/Suedseiten nicht
  // unterschieden — das wuerde eine Licht-Richtung unabhaengige Karte brauchen
  float moss = smoothstep(0.58, 0.86, fbm(vec2(u * 0.9 + 3.0, v * 1.3)))
             * (1.0 - smoothstep(0.05, 0.20, relH));
  albedo = mix(albedo, vec3(0.18, 0.28, 0.12), moss * 0.55);
  // duenne Aeste: gruenlich-braun, weniger Kontrast
  albedo = mix(albedo, vCol.rgb * (0.85 + 0.3 * coarse), 1.0 - onTrunk);
  float dif = max(dot(nb, SUN), 0.0) * 0.85 + 0.15;
  vec3 amb = vec3(0.40, 0.50, 0.68) * (0.45 + 0.55 * max(nb.y, 0.0));
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
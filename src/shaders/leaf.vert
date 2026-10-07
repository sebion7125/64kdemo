#version 330 core
layout(location=0) in vec2 aBase;   // (t laengs, w quer) Grundblatt
layout(location=1) in vec3 iPos;    // Blattbasis (Welt)
layout(location=2) in vec4 iData;   // size, rotY, tilt, lod-Pad
layout(location=3) in vec3 iCol;
uniform mat4 uMVP;
uniform vec3 uCamPos;
uniform vec3 uLodCam;   // Referenzpunkt fuer den Entfernungs-LOD (IMMER die
                        // echte Kamera — auch im Schatten-Wurf, sonst cullt der
                        // Wurf sich selbst die Blaetter weg)
uniform float uTime;
out vec3 vNrm;
out vec3 vPos;
out vec3 vCol;
out float vT;
out float vW;      // quer ueber das Blatt (-0.5..0.5) -> Silhouette im Fragment-Shader
out float vAsp;    // Seitenverhaeltnis Breite/Laenge (Sapling: niemals 1)
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
void main(){
  float size = iData.x, rotY = iData.y, tilt = iData.z;
  float t = aBase.x, w = aBase.y;
  float d = length(iPos - uLodCam);
  // LOD: die Kronen-Quads gibt es immer (fuelle die Krone), die echten Blätter
  // sterben in der Distanz statistisch aus — per Hash pro Blatt, damit die Dichte
  // sanft faellt und nicht schlagartig (Anzahl wechselt ja nur beim Rebuild).
  float keep = 1.0 - smoothstep(20.0, 28.0, d);
  if(hash12(vec2(iPos.x * 31.7, iPos.z * 17.3)) > keep) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
  float lod = 1.0 - smoothstep(24.0, 30.0, d);
  if(lod < 0.004){ gl_Position = vec4(2.0, 0.0, 0.0, 1.0); return; }
  // lokales Blatt: Laenge +X, Breite +Z, Normal +Y
  vec3 lp = vec3(t * size * lod, 0.0, w * size * lod);
  // Ueberhang (Rotation um Z): Spitze haeengt
  float ct = cos(tilt), st = sin(tilt);
  lp = vec3(lp.x * ct, -lp.x * st, lp.z);
  // leiser Biegung
  lp.y += t * t * size * 0.22;
  // Flattern des Einzelblatts (lokal, hohe Frequenz, staerker bei Boen)
  float ph = hash12(floor(iPos.xz / 15.0)) * 6.283;
  vec2 gp = iPos.xz * 0.10 - vec2(0.35, 0.20) * uTime;
  float gust = vnoise(gp * 2.3 + 7.7) * smoothstep(0.55, 0.85, vnoise(gp));
  lp.x += sin(uTime * 5.1 + ph * 3.7 + size * 91.0) * (0.10 + 0.40 * gust) * size;
  lp.y += cos(uTime * 4.3 + ph * 2.1 + size * 57.0) * (0.06 + 0.25 * gust) * size;
  // Drehung um Y
  float cr = cos(rotY), sr = sin(rotY);
  lp = vec3(lp.x * cr + lp.z * sr, lp.y, -lp.x * sr + lp.z * cr);
  vec3 p = iPos + lp;
  // Aest-Bewegung in WELT-Koordinaten: derselbe Ort wie in tree.vert -> dieselbe
  // Phase, Blatt und Ast schwingen also zusammen statt gegeneinander.
  float wnd  = sin(uTime * 1.8 + ph + iPos.x * 0.35 + iPos.z * 0.27);
  float wnd2 = cos(uTime * 1.3 + ph * 1.7 + iPos.z * 0.41);
  float swy = 0.55 + 0.45 * t;               // Spitze mehr als Blattbasis
  p.x += (wnd * 0.05 + gust * 0.17) * swy;
  p.z += (wnd2 * 0.04 + gust * 0.11) * swy;
  vec3 n = vec3(st * cr, ct, -st * sr);
  vNrm = n;
  vPos = p;
  vCol = iCol;
  vT = t;
  vW = w;
  vAsp = iData.w > 0.01 ? iData.w : 1.0;
  gl_Position = uMVP * vec4(p, 1.0);
}
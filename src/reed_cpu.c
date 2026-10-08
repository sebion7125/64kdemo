// Schilf-Pass (TODO 26): eigene Instanz-Puffer, eigene Geometrie.
//
// Warum nicht im Gras-Pass: eine Gras-Instanz ist EIN Quad. "Halm mit mehreren
// sitzenden Blaettern" ist damit nicht darstellbar — vier Anlaeufe (Runde 23b-d)
// sind genau daran gescheitert (Halm dicker = fettes Grasblatt; Blatt am Halm
// hochlaufen = Grasblatt mit Knick). Hier ist eine Instanz EINE PFLANZE:
// Culm + 4 Blaetter + Kolben, zusammengenaeht aus Streifen (siehe reed.vert).
//
// Platzierung: die CPU liefert nur die Gelaende-Hoehe (0,05-0,80 m ueber der
// Wasserlinie) auf einem Grid um die Kamera. Ob an einem Punkt eine Pflanze
// steht, entscheidet der Shader mit DEMSELBEN Feld reedW(x,z,y) wie die
// Boden-Toenung in fragment.glsl — sonst passen Schilfguertel und Toenung nicht
// zusammen (Plan STATUS.md, Runde 23e).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "scene.h"

float reedInst[REED_CAP * REED_FLOATS];
int   nReedInst = 0;
int   reedBuilds = 0;

int   reedRange  = 35;      // Grid-Halbseite um die Kamera [m] (DEMO_REEDRANGE)
float reedGridM  = 0.60f;   // Gelaende-Raster [m] (DEMO_REEDGRID)
float reedPerCell = 70.0f;  // Pflanzen pro Rasterzelle (DEMO_REEDPER)

// Projekt-eigener Hash (CPU-Zwilling von hash12(vec2) in den Shadern). Ein
// eigener sin()-Hash warf bei Weltkoordinaten (~1e5 im Argument) Precision weg
// und erzeugte Kurzzyklen: die Pflanzen einer Zelle stapelten sich.
static float rhash2(float x, float z){ return c_hash12(x, z); }

// Dichte-Feld: dieselben Funktionen wie in reed.vert / grass.vert (c_vnoise,
// c_hash12 sind die dokumentierten CPU-Zwillinge der GLSL-Versionen).
static float reedField(float x, float z, float y){
  float dh = y - WATER_Y;
  if(dh > 0.45f) return 0.0f;
  float px = x * 0.05f + 3.7f, pz = z * 0.05f;
  float patch = 0.5f * c_vnoise(px, pz) + 0.25f * c_vnoise(px * 2.03f + 17.1f,
                                                           pz * 2.03f + 9.2f);
  patch *= 1.3333f;
  return (1.0f - c_smoothstep(0.05f, 0.45f, dh)) * c_smoothstep(0.42f, 0.60f, patch);
}

// Baut das Grid um (cx,cz). Das Gelaende wird pro ZELLE (0,5 m) einmal
// abgetastet, die Pflanzen liegen gestreut darin — c_terrainH kostet 0,76 us
// (gemessen), ein Raster pro Pflanze (0,15 m) waeren 160 000 Abtastungen =
// 120 ms pro Build. So sind es 5 000 Abtastungen = ~4 ms.
//
// Der Dichte-Cull (Feld * 0,46 * Dichte gegen denselben Hash wie im Shader)
// laeuft hier schon mit: ohne haette die CPU 41 823 Instanzen gebaut, von denen
// der Shader 38 122 wieder verworfen haette (gemessen) — purer Vertex-Aufwand.
// (Der Shader prueft weiterhin selbst; CPU und Shader nutzen dieselben Felder.)
void reed_build(float cx, float cz, float dens){
  int n = 0, cells = 0, inBand = 0;
  int half = (int)((float)reedRange / reedGridM);
  for(int iz = -half; iz <= half; iz++){
    for(int ix = -half; ix <= half; ix++){
      float x0 = cx + (float)ix * reedGridM;
      float z0 = cz + (float)iz * reedGridM;
      float y = c_terrainH(x0, z0);
      float dh = y - WATER_Y;
      cells++;
      if(dh < 0.05f || dh > 0.80f) continue;   // nur die Uferzone
      inBand++;
      // Dichte pro Zelle EINMAL rechnen (das Feld hat ~20 m Wellenlaenge, ueber
      // 0,5 m aendert es sich kaum) — pro Pflanze waeren es 448 000 Noise-
      // Auswertungen statt 5 000 (gemessen: Build 49 ms -> ~7 ms).
      float wCell = reedField(x0, z0, y) * 0.46f * dens;
      if(wCell <= 0.001f) continue;
      // Pflanzenzahl pro Zelle: Mittelwert reedPerCell, stochastisch gerundet
      float hf = rhash2(x0 * 0.271f, z0 * 0.711f);
      int npl = (int)(reedPerCell * (0.4f + 0.6f * hf));
      if(c_hash12(x0 * 0.917f, z0 * 0.311f) < reedPerCell * (0.4f + 0.6f * hf) - (float)npl) npl++;
      for(int b = 0; b < npl && n < REED_CAP; b++){
        float jx = rhash2(x0 * 3.11f + b * 7.7f, z0 * 5.17f);
        float jz = rhash2(x0 * 2.13f + b * 3.3f, z0 * 4.37f + 5.5f);
        float x = x0 + (jx - 0.5f) * reedGridM * 0.95f;
        float z = z0 + (jz - 0.5f) * reedGridM * 0.95f;
        if(c_hash12(x * 7.31f, z * 11.7f) > wCell) continue;   // hier steht keine Pflanze
        float *d = &reedInst[n * REED_FLOATS];
        d[0] = x; d[1] = y - 0.03f; d[2] = z;                   // leicht eingegraben
        d[3] = 0.22f + 0.30f * rhash2(x * 3.71f, z * 5.13f);    // Hoeshe-Faktor
        // Halbe Halm-Breite [m]. Echtes Schilf: Halmdurchmesser 5-10 mm.
        // Vorher 26-48 mm (= Halbdurchmesser!) -> der Halm war 7-12 cm dick,
        // fuer den Nutzer "Porree-Stange". 7-13 mm gewaehlt: der Bandzug ist
        // kamera-zugewandt, unter ~7 mm verschwindet er jenseits von 8 m im
        // Aliasen (38 px/m auf 10 m bei 360 px Höhe).
        d[4] = 0.007f + 0.006f * rhash2(x * 1.13f, z * 7.31f);  // Breiten-Faktor [m]
        d[5] = 6.283185f * rhash2(x * 2.71f, z * 1.37f);        // yaw
        d[6] = 6.283185f * rhash2(x * 5.77f, z * 2.11f);        // Phase (Wind)
        d[7] = rhash2(x * 8.31f, z * 3.19f);                    // Variante
        n++;
      }
    }
  }
  nReedInst = n;
  reedBuilds++;
  printf("Schilf-Build: %d Pflanzen in %d Zellen (Uferzone %d), Radius %d m, "
         "Raster %.2f m, %.0f Pflanzen/m2 in der Uferzone\n",
         n, cells, inBand, reedRange, reedGridM,
         inBand ? (float)n / ((float)inBand * reedGridM * reedGridM) : 0.0f);
}

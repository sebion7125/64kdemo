# Gras-Verbesserungen – Arbeitsplan

Ziel: Das instanzierte Gras (`src/grass_cpu.c`, `src/shaders/grass.vert`, `src/shaders/grass.frag`)
näher an moderne Spiele heranbringen. Der Unterschied zu AAA-Titeln liegt kaum in Texturen,
sondern vor allem in **Beleuchtung** und **Verteilung**.

## Regeln für die Umsetzung

- **Ein Schritt pro Commit.** Nach jedem Schritt bauen, Screenshot machen, mit dem vorigen vergleichen, committen.
- Nur die genannten Dateien ändern. Kamera, Terrain, Bäume, HUD nicht anfassen.
- Bestehende Dateien nur mit gezielten Edits ändern, nicht komplett neu schreiben.
- GLSL bleibt `#version 330 core`. Keine neuen Uniforms, wenn es ohne geht.
- Alle Werte unten sind **Startwerte zum Abstimmen**, keine festen Vorgaben.
- Größenbudget beachten: Das Programm muss unter 65 536 Byte bleiben (mit `-Os` und `-s` gebaut).
- Shader-Funktionen (`hash12`, `vnoise`, `fbm`) nicht neu erfinden, sondern aus `src/shaders/fragment.glsl` übernehmen, damit Gras und Terrain identisch rechnen.

## Ausgangslage im Code (zur Orientierung)

- `grass.vert`: Halm aus 9 Basis-Vertices, `aBase = (t, ww)` mit `t` = Höhe entlang des Halms (0 = Fuß, 1 = Spitze), `ww` = Seite (−0,5 = linker Rand, +0,5 = rechter Rand).
- Instanzdaten: `iPos` (Fußpunkt), `iData = (Höhe, Breite, Wind-Phase, Schattierung)`, `iData2 = (yaw, lean)`.
- Die Halmebene ist um `yaw` gedreht. Die Breitenrichtung (Halm-„rechts“) ist
  `R = normalize(vec3(cos(yaw) - 0.7*sin(yaw), 0.0, sin(yaw) + 0.7*cos(yaw)))`
  (folgt direkt aus der `woff`-Rechnung im Vertex-Shader).
- `grass.frag`: `SUN` = normierte Richtung **zur** Sonne, `rd = normalize(uCamPos - vPos)` = Richtung **zur** Kamera. Farbverlauf `base → tip` über `vT` existiert schon (Selbstverschattung nach Höhe).

---

## Schritt 1 – Durchscheinen bei Gegenlicht (Transluzenz)

**Wirkung:** größte Verbesserung pro Codezeile. Halme sind dünn, Licht von hinten scheint durch. Wiesen „leuchten“ bei tiefer Sonne.

**Formel:** mit Blickrichtung $\mathbf{V}$ (von der Oberfläche zur Kamera, im Code `rd`), Lichtrichtung $\mathbf{L}$ (zur Sonne, im Code `SUN`), Schärfeexponent $p$ und Halmposition $t$ (0…1):

$$T = \max\bigl(0,\ (-\mathbf{V})\cdot\mathbf{L}\bigr)^{p} \cdot t$$

Der Faktor $t$ sorgt dafür, dass vor allem die dünnen Spitzen durchscheinen.

**Umsetzung in `grass.frag`** (nach der Berechnung von `col`, vor dem Nebel):

```glsl
float back  = max(dot(-rd, SUN), 0.0);
float trans = pow(back, 4.0) * vT;                 // p = 4 als Startwert
col += vec3(0.55, 0.65, 0.20) * trans * 0.8;       // Farbe/Stärke abstimmen
```

**Prüfen:** Kamera Richtung Sonne drehen → Halmspitzen hellgelbgrün. Von der Sonne weg → keine Änderung.

---

## Schritt 2 – Abgerundete Normalen

**Wirkung:** Der flache Halm schattiert wie ein leicht gewölbter. Die Licht-/Schattenkante läuft über die Halmbreite statt hart pro Halm umzuschalten.

**Idee:** Die Flächennormale des Halms wird je nach Seite (`ww`) zur Breitenrichtung $\mathbf{R}$ gekippt:

$$\mathbf{N} = \operatorname{normalize}\bigl(\mathbf{F} + 2k\,w_w\,\mathbf{R} + u\,\mathbf{Y}\bigr)$$

mit Flächennormale $\mathbf{F}$ (senkrecht zu $\mathbf{R}$, horizontal), Seitenparameter $w_w\in[-0{,}5;\ 0{,}5]$, Wölbungsstärke $k$, Aufwärtsanteil $u$ und Hochachse $\mathbf{Y}=(0,1,0)$.

**Umsetzung:**

- `grass.vert`: zusätzliches `out float vSide;` und `vSide = aBase.y;`. Die Normale neu aufbauen:
  ```glsl
  vec3 R = normalize(vec3(cy - 0.7*sy, 0.0, sy + 0.7*cy));
  vec3 F = vec3(-R.z, 0.0, R.x);
  // Wind-Neigung wie bisher als kleine Störung beibehalten
  vNrm = F + vec3(-wnd * 0.25, 0.0, -wnd2 * 0.18);
  vR   = R;                                   // out vec3 vR;
  ```
- `grass.frag`:
  ```glsl
  in float vSide; in vec3 vR;
  vec3 n = normalize(vNrm);
  if (dot(n, rd) < 0.0) n = -n;               // zur Kamera drehen (wie bisher)
  n = normalize(n + 2.0 * 0.6 * vSide * vR + 0.5 * vec3(0.0, 1.0, 0.0));  // k = 0.6, u = 0.5
  ```

**Prüfen:** Aus der Nähe: Halme zeigen einen weichen Hell-Dunkel-Verlauf quer zur Breite. Keine schwarzen Halme (sonst `u` erhöhen).

---

## Schritt 3 – Schwaches Glanzlicht

**Wirkung:** Halme sind leicht wachsig. Ein dezentes Highlight macht die Wiese bei Sonne lebendiger.

**Formel (Blinn-Phong):** Halbvektor $\mathbf{H}=\operatorname{normalize}(\mathbf{L}+\mathbf{V})$, Glanzexponent $s$, Stärke $k_s$:

$$S = k_s\,\max(0,\ \mathbf{N}\cdot\mathbf{H})^{s}$$

```glsl
vec3 H = normalize(SUN + rd);
float spec = pow(max(dot(n, H), 0.0), 32.0) * 0.15 * vT;   // s = 32, ks = 0.15
col += vec3(1.0, 0.95, 0.8) * spec;
```

**Prüfen:** Nur dezente Glanzpunkte, keine weißen Flächen. Im Zweifel `ks` senken.

---

## Schritt 4 – Büschel (Clumping)

**Wirkung:** Gleichmäßig zufällig verteiltes Gras wirkt künstlich. Echte Wiesen wachsen in Büscheln, deren Halme Höhe, Richtung und Neigung teilen.

**Idee (Voronoi):** Die Ebene wird in ein grobes Gitter (Zellgröße $C\approx 1{,}2$ m) geteilt, jede Zelle bekommt per Hash einen Büschel-Mittelpunkt. Für jeden Halm wird der nächste Mittelpunkt in den 3×3-Nachbarzellen gesucht. Der Halm übernimmt dessen Eigenschaften (Höhe, Richtung, Neigung) teilweise, abhängig vom Abstand zum Mittelpunkt.

**WICHTIG: Halm-Positionen NICHT verschieben.** Ein früherer Entwurf zog die Halme zum Mittelpunkt. Das erzeugte Lücken zwischen den Büscheln (einzelne Halme sichtbar) und machte die Dichte über `GSTEP`/Halmzahl unregelbar. Auch in den Quellen (GodotGrass) beeinflusst das Zellrauschen nur Drehung, Höhe und Biegung.

**Umsetzung in `grass_cpu.c`**, in `build_cell()`, nach Berechnung von `bx, bz` und vor dem Schreiben der Instanzdaten:

```c
#define CLUMP 1.2f
// naechsten Bueschel-Mittelpunkt suchen
int cix = (int)floorf(bx / CLUMP), ciz = (int)floorf(bz / CLUMP);
float best = 1e9f, cxm = bx, czm = bz, cid = 0.0f;
for (int j = -1; j <= 1; j++)
  for (int i = -1; i <= 1; i++) {
    float hx = c_hash12((cix + i) * 0.731f, (ciz + j) * 0.219f);
    float hz = c_hash12((cix + i) * 0.219f, (ciz + j) * 0.731f);
    float mx = (cix + i + hx) * CLUMP, mz = (ciz + j + hz) * CLUMP;
    float d2 = (bx - mx) * (bx - mx) + (bz - mz) * (bz - mz);
    if (d2 < best) { best = d2; cxm = mx; czm = mz; cid = c_hash12(mx, mz); }
  }
(void)cxm; (void)czm;   // Position bleibt unveraendert!
```

Nach dem Schreiben der Instanzwerte `d[3]`, `d[7]`, `d[8]` diese weich zum Büschel hin mischen:

```c
float dc = sqrtf(best) / CLUMP;                      // 0 = Mitte, ~0.7 = Rand
float w  = 1.0f - c_smoothstep(0.0f, 0.7f, dc);      // Bueschel-Einfluss 1..0
d[3] *= 1.0f + 0.25f * w * (2.0f * cid - 1.0f);      // Hoehe, Mittelwert bleibt ~1
float yawC = cid * 6.283f;
d[7] += w * 0.6f * remainderf(yawC - d[7], 6.283f);  // Richtung, kuerzester Winkelweg
d[8] *= 0.8f + 0.4f * w;                             // Neigung
```

- Höhenmodulation symmetrisch um 1 → mittlere Bedeckung und Dichte bleiben gleich.
- Effekt zu schwach? `0.25f` bzw. `0.6f` erhöhen, **nicht** die Position anfassen.

**Prüfen:** Halmzahl (`GRASS` im HUD) exakt gleich wie vorher. Keine Lücken, keine einzeln hervorstehenden Halme. Aus mittlerer Entfernung leichte, weiche Struktur (Höhen- und Richtungsflecken).

---

## Schritt 5 – Grundfarbe aus dem Terrain

**Wirkung:** Der Übergang vom Gras zur Terrain-Grasfarbe in der Ferne verschwindet. Farbflecken der Wiese setzen sich im Gras fort.

**Umsetzung in `grass.frag`:** Die Halmfarbe aus derselben Formel ableiten wie `terrainColor()` in `fragment.glsl`:

```glsl
// identisch zu terrainColor() in fragment.glsl (fbm von dort uebernehmen!)
float g = fbm(vPos.xz * 0.9);
vec3 ground = mix(vec3(0.22, 0.42, 0.13), vec3(0.48, 0.60, 0.18), g);
vec3 albedo = mix(ground * 0.6, ground * 1.3, vT * vT) * (0.75 + 0.5 * vCol.x);
```

**Wichtig:** `fbm` in `grass.frag` hat derzeit **3** Oktaven, in `fragment.glsl` (Zeile ~28) **4**. Für passende Farben muss es dieselbe Funktion sein: Für die Grundfarbe eine 4-Oktaven-Variante verwenden (die 3-Oktaven-`fbm` für `skyColor` kann bleiben).

**Prüfen:** Gras-Rand in der Ferne geht ohne sichtbare Farbstufe ins Terrain über.

---

## Schritt 6 – Wind als wandernde Böen

**Wirkung:** Statt jeder Halm schwingt für sich, laufen Windböen sichtbar über die Wiese.

**Idee:** Die Windstärke kommt aus einem Rauschfeld, das mit der Zeit in Windrichtung $\mathbf{w}$ (Einheitsvektor) mit Geschwindigkeit $v$ verschoben wird:

$$s(\mathbf{x}, t) = \text{vnoise}\bigl(0{,}08\,\mathbf{x} - \mathbf{w}\,v\,t\bigr)$$

mit Fußpunkt $\mathbf{x}$ (`iPos.xz`) und Zeit $t$ (`uTime`).

```glsl
// in grass.vert: vnoise/hash12 aus fragment.glsl uebernehmen
const vec2 WDIR = vec2(0.8, 0.6);
float gust = vnoise(iPos.xz * 0.08 - WDIR * uTime * 1.5);       // 0..1
float bend = (0.3 + 1.2 * gust);                                  // Grundwind + Boeen
p.x += WDIR.x * bend * 0.12 * t * t * lod;
p.z += WDIR.y * bend * 0.12 * t * t * lod;
// bisheriges Flattern (wnd/wnd2) mit kleinerer Amplitude behalten
```

**Prüfen:** Hellere/dunklere Wellen laufen gleichmäßig in eine Richtung über die Wiese.

---

## Schritt 7 (optional, aufwendig) – Schatten der Bäume auf dem Gras

Moderne Spiele nutzen Shadow Maps (Tiefenbild aus Sonnensicht). Das wäre ein eigener Render-Pass und ein größerer Umbau.

Günstige Näherung für dieses Projekt: Im Vertex-Shader prüfen, ob der Halm im Schatten einer Baumkrone liegt (Krone als Kugel, Strahl vom Halm Richtung `SUN`). Dafür bräuchte der Shader die Baumpositionen (z. B. als Uniform-Array der nächsten Bäume oder als kleine Textur). Erst angehen, wenn Schritt 1–6 fertig sind.

---

## Nicht verifiziert / bewusst weggelassen

- Ghost of Tsushima soll Halme, die man genau von der Kante sieht, leicht zur Kamera drehen, damit sie nicht zu Strichen werden. Das ist **nicht nachgeprüft**. Bei Bedarf im GDC-Vortrag nachsehen, bevor es umgesetzt wird.
- Texturen auf Halmen sind nicht nötig. Moderne Titel nutzen Geometrie plus prozedurale Farbe, wie dieses Projekt.

## Quellen

- Sucker Punch Productions: *Procedural Grass in „Ghost of Tsushima“*, GDC 2021, Advanced Graphics Summit. https://gdcvault.com/play/1027214/Advanced-Graphics-Summit-Procedural-Grass
- AMD GPUOpen: *Procedural grass rendering* (Mesh Shaders; Bézier-Halme, LOD mit Verbreiterung, Selbstverschattung nach Höhe, Normalen-Interpolation). https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
- 2Retr0: *GodotGrass* (Nachbau nach dem Tsushima-Vortrag; abgerundete Normalen, Subsurface-Scattering-Näherung, Clumping mit Zellrauschen, Wind mit wanderndem Rauschen). https://github.com/2Retr0/GodotGrass

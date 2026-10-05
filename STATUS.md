# 64kDemo — Status

Stand: 2026-10-05, nach Gras-Optik-Session (Schritte 1,3-6) + 64k-Budget.

## ⚡ SOFORT NÄCHSTER SCHRITT

**Visuelle Verifizierung durch den Nutzer** (Gras-Optik-Änderungen dieser
Session): Transluzenz (in Sonnenrichtung schauen), Specular, Clumping
(Büschel + freie Flecken), Grundfarbe aus dem Terrain (Basis nimmt
Gras-/Sandfarbe an), wandernde Böen (Wind driftet ~3.5 m/s).

Danach optional: **Schritt 7 (Baum-Schatten auf Gras)** aus dem
7-Schritte-Plan (vorher: 1=Transluzenz, 2=Normalen [schon vor der Session
davon], 3=Specular, 4=Clumping, 5=Grundfarbe, 6=Böen — alle erledigt).

**64k-Budget: ERREICHT — 64,032 Byte** (Ziel < 65,536 mit -Os -s, jetzt
zusätzlich -no-pie -fno-asynchronous-unwind-tables -Wl,-z,noseparate-code).

### Kontext: was diese Session alles verändert hat

- **3 Slider** (unten Mitte, Maus ziehen): Dichte 0.2–8.0x (grün, unten),
  Halmhöhe 0.3–3.0x (gelb-grün, Mitte), max. Gras-Sichtweite 40–120m
  (blau, oben). HUD zeigt `D x.xfx H x.xfx MD xx.xm`.
- **AA 8x**: M-Taste zyklisch 4x → 8x → OFF (`msaaSamples`, FBO-Recreate via
  `fboW=0`).
- **Gras-Rebuild-Trigger**: `camTgt` → `camPos` (Drehung zählt nicht mehr,
  nur Positionswechsel >12m). Build-Zentrum = camPos, Radius =
  `grassMaxDist + 20m` (Margin, damit bei Drehung alles "da" ist).
  Bäume bleiben bei camTgt.
- **Dichte-Falloff**: `pow(1-smoothstep(8, grassMaxDist, dist), 1.6)`, weicher
  Abbruch bei 0.01 (kein plötzliches Null). Nahbereich 4x dichter
  (`4+20*dens` statt `1+5*dens`, Cap 12/Zelle). GRASS_CAP 1.5M.
- **Per-Halm-Fade-in**: GRASS_FLOATS 9→10 (Birth-Time), VAO attr4 @ offset 36,
  `gWorldTime` (extern, main.c setzt pro Frame).
- **FPS-Anzeige**: Frame 1 ignorieren, Frame 2 = erster echter Abstand als
  Basis, ab Frame 3 EMA 0.95/0.05.
- **Screenshot-Funktion** (`save_screenshot_ppm`): `glReadBuffer(GL_BACK)`
  VOR `glReadPixels` (war nachher!), `glFinish()` vorher, w/h-Check,
  `glGetError`-Check. `p_glFinish` neu geladen.
- **Defensive Checks**: nGrass<1 → 1 vor VBO-Upload, `glFinish()` nach
  Gras-VBO-Upload (Race-Verdacht, Mesa-Threads waren im Coredump aber idle).

---
## Architektur (aktuell)

**Hybrid-Rendering:**
- **Raymarch** (fragment.glsl): Himmel, Wolken, Terrain, Wasser (einschliesslich Reflexionen)
- **Raster** (GL 3.3, VAOs): Bäume (Stamm/Aste/Tüpfel als Mesh) + Blattdach (Billboard-Quads mit prozeduraler Blatt-Textur + Alpha-Test) + Gras (instanzierte Halme)
- **Compositing** per HW-Depth-Test: Szene-Pass (Raymarch) schreibt `gl_FragDepth` (exakte Projektionstiefe) zuerst, Mesh-Pass rendert danach mit Depth-Test dagegen. (Früher: per-Shader-Compositing im Raymarch-Pass.)

**CPU-Module (reines C, kein GL):**
- `terrain_cpu.c` — Terrain-Höhe (1:1-Port des Shaders)
- `trees_cpu.c` — Baum-Geometrie (429 Bäume, ~386k Vertices, 7722 Canopy-Quads)
- `grass_cpu.c` — Gras-Halme (GRASS_CAP 1.5M, inkrementeller Zweipuffer-Rebuild)
- `main.c` — GL-Setup, Mesh-Pass, Raymarch-Pass, Compositing, Kamera, HUD

**Kamera:** Free-Fly (Pointer-Lock, Halten-links = Look, WASD, Space/X hoch/runter, Shift Turbo, O Orbit, R Reset). Pitch ±83° Clamp, invertiert, Sensitivität 0.0008.

## Gefixt (Session 2026-10-05, Gras-Optik + 64k-Budget)

- [x] **Gras-Optik Schritt 1 (Transluzenz)**: `pow(max(dot(-rd,SUN),0),3)`
  * (0.3+0.7*vT) * 0.55 mit Halm-Albedo — Gegenlicht, Spitze heller.
- [x] **Schritt 3 (Specular)**: Blinn-Phong, Exponent 24, Faktor 0.12,
  Spitzen staerker (dezent: Pixel = mehrere Halme).
- [x] **Schritt 4 (Clumping)**: 8m-Grid, pro Zelle Hash-Cluster-Mittelpunkt
  + Dichte-Varianz, nb * (0.21..1.0) nach Distanz zum Mittelpunkt.
  Haengt nur von (x,z) ab -> Zell-Kopieren bleibt gueltig. Halmzahl
  ~40% (freie Flecken), Build schneller (246k statt 610k Halme).
- [x] **Schritt 5 (Grundfarbe)**: Halm-Basis = Terrain-Grasfarbe
  (4-Oktaven-fbm, gleiche Werte/Scale wie terrainColor) + Sand am Ufer.
  Basis und Terrain nutzen dieselbe Formel -> LOD-UEbergang nahtlos.
  fbm in grass.frag auf 4 Oktaven (Consistency; wirkt auch auf Wolken).
- [x] **Schritt 6 (Boen)**: vnoise im mitbewegten Koordinatensystem
  (~3.5 m/s) * Front (smoothstep 0.55..0.85); Basis-Brise bleibt;
  Normale neigt sich mit der Boe. hash12/vnoise wie in fragment.glsl.
- [x] **64k-Budget: 64,032 Byte** (war 76,056 bei -Os -s). Hebel:
  -no-pie + -fno-asynchronous-unwind-tables + -Wl,-z,noseparate-code
  (Linker-Layout/Padding) + Slider-Helfer + 1 MSAA-Blit + 4 ungenutzte
  GL-Funktionen weg + kuenzere Strings + Shader-Minify (Minifier:
  Kommentare/Leerzeilen/Literale, ein String-Literal pro Shader).
- [x] Makefile: Groessen-Check nach dem Build (Warnt bei > 65,536).

## Gefixt (Session 2026-10-05, 1m-Trigger)

- [x] **Gras-Rebuild-Trigger 12m → 1m** + Rebuilds 4.5x schneller:
  - **Zell-Kopieren**: Halm-Daten hängen nur von (ix,iz,b) ab (Hashes), NICHT
    von der Distanz (Größe ist im Shader). Zellen mit unverändertem nb werden
    kopiert statt regeneriert (per-Zelle Offset/Count-Tabellen, CT=700,
    ~5MB BSS). 1m-Shift: ~150ms statt ~700ms (VM-Messung; Rest-Kosten sind
    Loop+memcpy, VM-laufzeitgebunden).
  - **Build-Radius = Dichte-Cutoff** (Bisektion, ~70m bei MD=80) statt
    MD+20: Zellen außerhalb generieren keine Halme (dens<0.01), der Margin
    hat nur Build-Zeit gekostet.
  - **Dichte-LUT** (0.5m-Resolution, pro MD gecacht) statt powf pro Zelle.
  - **Worker: "finish then latest"** (kein Mid-Build-Restart): Build wird
    immer zu Ende gebaut, nächster geht direkt zum neuesten Zentrum.
  - Step-Loop: (ix,iz) inkrementell statt div/mod (idivl).
  - Folge: Gras hinkt bei 13 m/s ~2m hinterher, bei Turbo (34 m/s) ~5m,
    nach Stopp konvergiert in ~150ms. Kopierte Halme behalten ihre
    Birth-Time (Fade läuft nahtlos weiter).

## Gefixt (Session 2026-10-05, Thread/Fade/Breite/Terrainfarbe)

- [x] **Gras-Fade-in war totes Brett** — Wurzelursache: `grass_build_finish`
  hatte "neu" = außerhalb des alten **Build-Radius (100m)** definiert, aber
  Halme existieren nur innerhalb **~70m** um das Build-Zentrum (dens<0.01-
  Cutoff). Bei <30m Kamerawechsel → 0 neue Halme → nichts fadete. Fix: "alt" =
  Halm im alten **Dichte-Bereich** (gleiche Bedingung wie `build_cell`).
  Nebensächlicher Fix: `hasOldBuild` wurde in `begin` statt `finish` gesetzt →
  erster Build wurde gegen (0,0) verglichen (toter else-Zweig). Verifiziert:
  12m-Sprung → 38.361 Halme blenden 2s ein (vorher: 0). Swap-Output:
  `Gras-Swap: N Halme, M neu`.
- [x] **Halm-Breite/-Höhe springt beim Rebuild** — `sizeScale` aus dem Build
  genommen (CPU speichert nur Basis-Höhe/-Breite), stattdessen im Shader
  (`grass.vert`) Distanz-basiert: `dens = pow(1-smoothstep(8, uMaxDist, d), 1.6)`,
  `sizeScale = 1+1.8*(1-dens)` zur aktuellen Kameradistanz → kontinuierlich,
  kein Sprung beim Swap.
- [x] **Gras-Generator in eigenem Thread** — Worker-Thread in `grass_cpu.c`
  (`grass_thread_start`), baut komplett (kein GL), **"latest wins"**: neuer
  Request während Build = Restart mit neuem Zentrum (Chunk-Check alle 16000
  Zellen). Main-Thread: `grass_build_request(cx,cz)` bei Trigger, Upload per
  `grass_build_finished_snapshot` (ein-malig) + `glFinish`. Erster Build bleibt
  synchron (Gras ab Frame 1). Makefile: `-lpthread`. Build-Dauer ~1.4s.
- [x] **Terrain-Grasfarbe an Halme angepasst** — Terrain-Albedo war ~7-15% heller
  (gelblicher) als der Halm-Mittelwert → Farbbruch beim LOD-Fade in der Ferne.
  Fix: `terrainColor`-Gras mit `* vec3(0.87, 0.93, 0.89)` (Halm-Mittelwert /
  Terrain-Mittelwert; Beleuchtung war in beiden Shadern identisch). Verifiziert
  per Screenshot-Vergleich: Terrain-Pixel exakt -13%/-7%/-11% (R/G/B).

## Gefixt (Session 2026-10-05, Sliders/Fade/Crash)

- [x] **Start-Crash (SEGV, Host+VM)** → **Wurzelursache: HUD-Buffer-Overflow.**
  HUD-String mit neuem "MD"-Feld bis 100 Zeichen, `hudVerts` war nur für
  96 (`HUD_MAX_CHARS`), `build_text_verts` ohne Bounds-Check → Speicher-Overflow
  → SEGV an scheinbar zufälligen Orten (glBufferData, gActive, Screenshot —
  sah aus wie Race, war aber deterministisch). Fix: `HUD_MAX_CHARS` 96→128 +
  Bounds-Check. (Coredump-Analyse: Mesa-Threads alle idle → kein Race.)
- [x] **FPS-Spike beim Start** (100000+ → runter): erster Frame-Abstand war
  künstlich klein. Fix: Frame 1 ignorieren, Frame 2 (erster echter Abstand)
  als Basis, ab Frame 3 EMA.
- [x] **Screenshot-Bugs**: `glReadBuffer` war NACH `glReadPixels` (vorher
  setzen), `glFinish()` davor, w/h-Check gegen minimiertes Fenster.
- [x] **Kamera-Drehung triggerte Gras-Rebuild** (camTgt als Trigger) → jetzt
  camPos (nur Positionswechsel). Build-Radius = Sichtweite + 20m Margin.
- [x] **MSAA 8x** (M-Taste: 4x→8x→OFF, `msaaSamples`, FBO-Recreate).
- [x] **3 Slider**: Dichte 0.2–8.0x, Halmhöhe 0.3–3.0x, Sichtweite 40–120m.
- [x] **Per-Halm-Fade-in** (Birth-Time, GRASS_FLOATS=10) — Mechanik drin,
  aber visuell noch nicht überzeugend (siehe SOFORT NÄCHSTER SCHRITT).

## Gefixt (diese Session)

- [x] **MVP z-Mapping** (P-Matrix hatte zb/-1 vertauscht → clip.w=0)
- [x] **HUD-Buffer-Overflow** (HUD_MAX_CHARS 96)
- [x] **Crown-Shader vec3*vec2-Bug**
- [x] **Compositing: Bäume am Himmel abgeschnitten** → "nächstes Objekt gewinnt"
- [x] **Schwarzer Himmel** (kein Mesh + Himmel → 50%-Blend mit schwarzem Mesh-Pixel) → kein Mesh = wm 0
- [x] **Blattdach nur vor Holz sichtbar** → Crown hatte `glDepthMask(GL_FALSE)` (schrieb keine Tiefe) → jetzt Depth-Write (Alpha-Test = discard, also korrekt)
- [x] **Wasserwellen axis-aligned** → chaotisch (7 Wellen, `dot(p, richtung)`, verschiedene Richtungen)
- [x] **Wellenlänge zu groß** → jetzt ~1.5–4m (3 mittlere + 4 feine Rippel)
- [x] **Gras-Dichte mit Distanz-Falloff** (nah dicht, fern lichter; Halme werden mit Distanz größer → einheitliche visuelle Dichte)
- [x] **Gras dichter** (0.30m-Zellen, 2–6 Halme/Zelle, GRASS_CAP 280k)
- [x] **HUD skaliert mit Auflösung** (hud_scale = h/720)
- [x] **MSAA schwarzes Bild** (Blit-ABI-Bug, siehe unten „MSAA-Wurzelursache") → `glBlitNamedFramebuffer` + GL 4.3-Kontext, M-Taste toggle, in VM verifiziert (AA an/aus = gleiche Szene, Helligkeit ~165 statt 0)
- [x] **AA dunkelt Polygon-Kanten ab** (Mesh+schwarzer Clear gemischt) → **Pipeline umgestellte**: Szene-Pass (Raymarch) erst, dann Mesh-Pass mit HW-Depth-Test dagegen. Kanten mischen jetzt Mesh+Landschaft.
- [x] **Bäume durch Hügel** (Verdeckung kaputt) → llvmpipe schreibt `gl_FragDepth` nur mit aktivem Depth-Test (siehe unten). Fix: Szene-Pass mit Depth-Test an + `GL_LEQUAL`.
- [x] **Presentation schwarz nach Umstellung** → Presentation brauchte explizit `vaoTri` binden + `glDisable(GL_DEPTH_TEST)` (vorher implizit über Szene-Pass).
- [x] **Gras nur am Bildschirmrand (VM)** → **Wurzelursache:** Gras-Basis-Geometrie hatte nur 5 Vertices, aber `GL_TRIANGLES` count=9 brauchte 9 → Vertices 5–8 out-of-bounds (GPU-Müll) → teilweise gerendert. VM-spezifisch, weil llvmpipe andere Müllwerte als NVIDIA. Fix: Geometrie auf 9 Vertices (duplizierte, ohne Index-Buffer).
- [x] **Gras-nur-Testmodus** (Taste V): nur Gras zeichnen (kein Terrain/Baeume/Blattdach), Magenta-Hintergrund. Hilfreich zur Isolation.

## MSAA-Wurzelursache (Forschung, 2026-10-04)

**Symptom:** Mit MSAA (useMSAA=1) komplett schwarzes Bild, ohne MSAA normal.

**Wurzelursache:** `glBlitFramebuffer` hat zwei ABIs — 10-Parameter (GL 3.0/ARB,
nutzt die *gebundenen* Read/Draw-FBs) und 12-Parameter (GL 3.2 core, explizite
FB-Handles). Mesa-GLX (die VM: `libGL.so.1` = eigenes Mesa-GLX, kein glvnd)
liefert für `glXGetProcAddress("glBlitFramebuffer")` in einem 4.5-Core-Kontext
den **10-Parameter-Entrypoint**. Der App-Code rief es mit der 12-Parameter-
Core-Signatur auf → Argumente verschoben (FB-Handles landeten in srcX0/dstY0,
filter bekam H) → Cross-FBO-Blit = stille No-Op → texScene blieb schwarz.

**Beweise:**
- Minimal-Test: FBOs COMPLETE, kein GL-Error, aber Blit kopiert nichts (Ziel-FBO
  bleibt bei Vorfarbe).
- `MESA_DEBUG=blit`: `GL_INVALID_ENUM in glBlitFramebuffer(invalid filter 0x78)`
  — 0x78 = 120 = H, d.h. das 10. Argument (10-Param-Mapping).
- Mesa-Quelle (branch 26.0): `_mesa_BlitFramebuffer` = 10 Param + ReadBuffer/
  DrawBuffer; `_mesa_BlitNamedFramebuffer` = 12 Param.
- **Fix verifiziert (VM):** `glBlitNamedFramebuffer` (eindeutige 12-Param-
  Signatur, GL 4.2+) funktioniert; ebenso 10-Param-Aufruf mit explizit
  gebundenen `GL_READ_FRAMEBUFFER`/`GL_DRAW_FRAMEBUFFER` — aber nur auf Mesa.

**Lösung:** Kontext auf GL 4.3 (Fallback 3.3, dann Legacy) + `glBlitNamedFramebuffer`
für den MSAA-Resolve. Funktioniert auf Mesa (VM) und NVIDIA (Host), weil die
Signatur eindeutig ist.

**Testprogramme** (in /tmp): `msaa_test.c` (Repro), `msaa_ctrl.c` (Kontroll-
experimente E1–E9), `blitmode.c` (10-Param-gebunden vs. Named-Framebuffer),
`fragdepth.c` (gl_FragDepth-Repro), `winblit.c` (Window-Draw-Test).

## Pipeline-Umstellung: Compositing per HW-Depth-Test (2026-10-04)

**Alt:** Mesh-Pass → fboMS, Resolve → fbo, dann Raymarch-Pass liest Mesh-Farbe/-Tiefe
als Textur und compostet per-Shader ("nächstes Objekt gewinnt", smoothstep-Blend).

**Neu:**
1. **Szene-Pass** (Raymarch) zuerst: Farbe + `gl_FragDepth` (exakte Projektionstiefe,
   zu `build_mvp` passend, +0.05m Bias damit Oberflächen-Mesh gewinnt) ins FBO.
2. **Mesh-Pass** (Bäume/Gras/Blattdach) mit HW-Depth-Test (`GL_LESS`) dagegen.
3. **Resolve** fboMS → fbo (nur bei MSAA), dann Presentation.

**Vorteil:** MSAA-Kanten mischen Mesh+Landschaft (nicht mehr Mesh+schwarzer Clear).
**Tonemapping:** Szene + alle Mesh-Fragment-Shader (tree/grass/crown) tonemappen jetzt
selbst (`1-exp(-c*1.25)`, `pow(c, 0.4545)`), da das Compositing nicht mehr linear im
Raymarch-Shader passiert.

### llvmpipe-Bug: `gl_FragDepth` ohne Depth-Test
**Symptom:** Szene-Pass mit `glDisable(GL_DEPTH_TEST)` → `gl_FragDepth` wird nicht
geschrieben (Depth-Buffer bleibt 1.0) → alle Meshes gewinnen den Depth-Test →
Bäume durch Hügel.

**Repro** (`/tmp/fragdepth.c`): `gl_FragDepth=0.5` + Depth-Test aus → Readback 1.000;
Depth-Test an → Readback 0.500. Spec: `gl_FragDepth` soll unabhängig vom Depth-Test
geschrieben werden (Test bestimmt nur, ob das Fragment gezeichnet wird).

**Workaround:** Szene-Pass mit `glEnable(GL_DEPTH_TEST)` + `glDepthFunc(GL_LEQUAL)`
(Himmel=1.0 muss gegen gecleartes 1.0 durchkommen). Mesh-Pass: `glDepthFunc(GL_LESS)`.
Funktioniert auch auf NVIDIA (dort wird `gl_FragDepth` immer geschrieben).

## Aktuell offen / To-Do

### 1. Gras (Gras-Optik-Schritte 1,3-6 + Budget sind fertig)
- [ ] Visuell verifizieren (Nutzer): Transluzenz, Specular, Clumping,
  Grundfarbe, Böen, Fade-in, Terrainfarbe-Übergang
- [ ] Optional: Schritt 7 (Baum-Schatten auf Gras)
- [ ] V-Modus-Crash auf Host neu testen (maximiert + AA off)
- [ ] Gras-Flackern sehr nah an der Kamera prüfen (Compositing/Depth)

### 2. Bäume
- [ ] **Nähte in den Röhren** (Tube-Segmente) → parallel transport frames
- [ ] Stamm-Textur (Rinde) verbessern

### 3. Wasser
- [ ] Neue Wellenlänge (~1.5–4m) vom Nutzer bewerten lassen
- [ ] Eventuell: Wellen normal-map für feineres Funkeln

### 4. MSAA
- [x] Wieder aktiviert (useMSAA=1), 8x via M-Taste möglich
- [ ] llvmpipe (VM): Root-Cause für schwarzes Bild mit MSAA finden (vermutlich Depth-Blit → DEPTH_COMPONENT24-Textur + Sampling im selben Frame)

### 5. Binärgröße
- [x] **unter 64 KB**: 64,032 Byte (siehe Commit "64k-Budget")
- [ ] Bei neuen Features: Budget-Check im Makefile warnt bei > 65,536

### 6. Sonstiges
- [ ] STATUS.md bei Änderungen aktuell halten
- [ ] `tools/test_trees.c` bei Grass-Änderungen aktualisieren (Gras-Test fehlt)

## Build & Test

```
cd /home/xvm/Projekte/64kDemo && make          # sauber, 0 Warnings (-Wall -Wextra)
./bin/64kdemo 1280 720                          # Screenshot → /tmp/64kdemo_shot.ppm
python3 -c "from PIL import Image; Image.open('/tmp/64kdemo_shot.ppm').save('/tmp/64kdemo_shot.png')"
```

- VM: Mesa/llvmpipe (DISPLAY=:0), Host: NVIDIA
- Debug: `gcc -O1 -g -Wall -Wextra -std=c11 -Ithird_party/include -o /tmp/64kdemo_dbg src/*.c -ldl -lm`
- gdb: `gdb -nx` (.gdbinit hat kaputte Breakpoints)

## Wichtige Konstanten

- `WATER_Y = 0.25`, `CAM_NEAR = 0.1`, `CAM_FAR = 300`
- Compositing-Blend-Zone: ±0.03m
- Gras: STEP 0.30m, Build-Radius = Dichte-Cutoff (~70m bei MD=80),
  Dichte-Falloff `pow(1-smoothstep(8, grassMaxDist, dist), 1.6)` (LUT),
  Halme/Zelle `4+20*dens` (Cap 12) * Clumping-Faktor (0.21..1.0, 8m-Grid),
  GRASS_CAP 1.5M, GRASS_FLOATS 10
  (pos3, hoehe, breite, phase, schattierung, yaw, lean, birth)
  — hoehe/breite = BASIS-Werte, Distanz-Skalierung im Shader (grass.vert)
  — Generator im eigenen Thread ("finish then latest"), Zell-Kopieren
    macht 1m-Rebuilds ~125ms, VBO-Upload im Main-Thread
  — Shader: Transluzenz, Specular, Grundfarbe aus Terrain (4-Oct-fbm),
    wandernde Boen (vnoise, ~3.5 m/s)
- Bäume: Rebuild >12m (camTgt), Gras: Rebuild >1m (camPos)
- Slider: Dichte 0.2–8.0x, Höhe 0.3–3.0x, Sichtweite 40–120m (Maus, unten Mitte)
- Tasten: M=AA (4x/8x/OFF), G=Gras-Freeze, V=Nur-Gras, P=Screenshot,
  T=Wireframe, O=Fly/Orbit, 1/2/3=Resolution, Q/Esc=Quit
- Caps: TREE_VERT_CAP 420000, CROWN_CAP 55000, GRASS_CAP 1500000, WIRE_CAP 90000
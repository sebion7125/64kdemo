# 64kDemo — Status

Stand: 2026-10-05, Runde 5 (Chat-Wechsel). **Gras-Fade-out: VAO-Fix +
Kanten-Pop-Fix committet und vom Nutzer bestätigt** (Slider ✓, Pop ✓ weg).
**Issue B (kein neues Gras nach ~77m) ist FIXED und vom Nutzer bestätigt.**
**Issue A (Längensprung/Ploppen in der Ferne) ist FIXED (VM-verifiziert),
wartet auf Nutzer-Host-Test.** Offen: Issue C (Gras-Inseln abweichend vom
Terrain). Details + Hypothesen + Testpläne unten in „ISSUES“.

## ⚡ SOFORT NÄCHSTER SCHRITT (nächster Chat)

**Issue A-Fix ist committet (Death-Time erst am Swap-Punkt, nicht während
des Builds). Nutzer-Host-Test anstehen:** gleiche Freeze-Fahrt wie vorher
(G an, rückwärtsfahren, G aus) → die sofortigen Sprünge am Build-Wechsel
sollten weg sein, stattdessen sanfte 2s-Fades. Falls noch ein
„Längensprung" sichtbar: Taste n + m (Halm-Dump mit DIFF) drücken.

Reihenfolge:
1. **Issue A:** Nutzer-Host-Test (siehe oben).
2. **Issue C (Inseln) in der VM suchen**: Halme-Höhen vs. c_terrainH an
   denselben Positionen dumpen; Raymarcher-SDF (0.85-Dämpfung) prüfen.
3. Danach Wahrnehmungsfrage (Fade sichtbar?) mit Nutzer abstimmen
   (Death-Fade 2s → 3–4s? langsamere Testfahrt?).
Danach wie bisher: Dichte/Farben/Weite, Entfernungssortierung, Budget,
Baum-Schatten.

## ISSUES (Nutzer-Beobachtungen 2026-10-05, nach Pop-Fix)

### Issue A: „Neue Halme ploppen in der Entfernung auf“ — **FIXED (VM-verifiziert), wartet auf Nutzer-Host-Test**
- **Beobachtung (Nutzer):** Freeze (G) an + rückwärtsfahren: die
  Shader-Grenze (LOD 68–80m) ist sichtbar und weich. Freeze aus: ein Teil
  des Grases fadet sichtbar aus (Death-Fade ✓), aber DANN ploppen in der
  Entfernung plötzlich Halme „auf“. Nutzer-Hypothothese: keine neuen Halme,
  sondern **nur ihre Länge springt**.
- **Wurzel (gemessen, VM):** Die Death-Time wurde WÄHREND des Builds
  gesetzt (gWorldTime im Kopier-Pfad), nicht beim Swap. Der Build dauert
  0.3–1s → beim VBO-Swap waren die sterbenden Halme bereits 0.3–1s im
  Fade (f ≈ 0.7–0.85 statt 1.0) → **sofortiger sichtbarer Sprung von
  15–30% am Swap-Punkt** (Pixelmessung: -34% in einem Frame vor Fix).
  Die „ploppenden“ Halme sind die NEUEN Halme (Fade-in über 2s vom Swap
  ab — korrekt), die aber erst ab ~30% Alpha (~0.5s) deutlich sichtbar
  werden → Wahrnehmung „ploppen plötzlich auf". Halme-Daten-Dump
  (Taste n/m) vor/nach Build: 0 Halme verschwinden ohne Death-Time,
  0 Höhen-Sprünge, Birth/Death korrekt → Regenerierung- und
  Reset-Hypothesen ausgeschlossen.
- **Fix (grass_cpu.c):** Death-Time erst beim Finish (Swap-Punkt) setzen:
  Kopier-Pfad + out_build_step schreiben Platzhalter -1.0f, grass_build_finish
  setzt d[10] = gWorldTime. (Birth-Time war schon korrekt am Finish.)
- **Verifiziert (VM, Pixel-Serien + b-Dumps mit exakten Zeitstempeln):**
  S-Szenario (Kamera vom Feld weg): ferne Zeile vor Fix -34% sofort am
  Swap; nach Fix sanft: -4% (0.4s) → -15% (1s) → -23% (1.4s) → stabil
  -11% (Death-Fade über 2s, dann neue Halme faden ein). W-Szenario
  (Kamera zum Feld hin): ferne Zeile steigt sanft um +13% über 2s
  (Fade-in), kein Sprung. Buffer-Seite (Summe-ShaderFade) passt exakt
  zur smoothstep-Kurve ab Swap-Zeit. Edge-Pop-Unit-Test: 0 Pops.
- **Offen:** Nutzer-Host-Test (gleiche Freeze-Fahrt wie vorher). Falls
  dort noch ein „Längensprung" sichtbar ist: Taste n (Punkt 45m vor +
  20m links der Kamera setzen, Halm-Dump) + m (gleiche Punkte erneut,
  DIFF) während der Szene drücken — zeigt Höhen/Birth/Death pro Halm.

### Issue B: Nach einer Strecke wird kein neues Gras mehr generiert, Halmzahl fällt nur — **FIXED + vom Nutzer bestätigt**
- **Beobachtung (Nutzer):** nach Zurücklegen einer Strecke ein Modus,
  in dem plötzlich gar kein neues Gras mehr generiert wird und die
  Halmzahl nur noch fällt.
- **BESTÄTIGT (VM-Repro):** 75s Geradeausfahrt Richtung +x/+z (Taste s):
  Walk 700×700, **dcx0=699** (Kamera am Quadrat-Rand!), Halmzahl
  kollabiert **257 → 118 → 52, 0 neu** — exakt das Nutzer-Symptom.
  Richtung -x/-z (Taste w): nur unsichtbarer Hinterkant-Rand (ok).
- **Wurzel:** CT=700 (210m) clamps das Union-Quadrat, behält aber die
  KLEINERE Kante (gix0/giz0 = ux0/uy0) → Richtung +x/+z bleibt die
  hintere Kante am Start PINIERT, die vordere (Generierung!) wird nach
  ~77m abgeschnitten; ab ~143m liegt das ganze Feld hinter der Kamera.
- **Fix (grass_cpu.c, grass_build_begin):** Beim Clamp Quadrat ZENTRIERT
  auf die Kamera (gix0 = floor(cx/GSTEP) - CT/2, gnx = CT). Mit
  clampedX/clampedZ-Flags: Sobald der Clamp greift, wird pro Build
  weiter zentriert (ohne Flags wäre das Quadrat 128m fixiert und es
  gäbe einen 38m-Fade-in-Sprung an der Kante). Front immer volle
  66,7m; verlorener Rand-Schimmer (~1m) poppt bei ~105m = jenseits des
  MD=80-Culls → unsichtbar.
- **Verifiziert (VM, 75s beide Richtungen):** dcx0=dcz0=350 (zentriert),
  +x/+z: 700k Halme, ~25k neu/Build (vorher: 52, 0 neu); -x/-z: 960k,
  ~16k neu. Pop-Unit-Test: 0 Pops. Build-Intervall ohne Regression.
- **Nutzer-Host-Test: BESTÄTIGT (2026-10-05)** — "B ist behoben".

### Issue C: Inseln, wo das Gras einer „abweichenden Insel“ folgt
- **Beobachtung (Nutzer):** Inseln, wo die Gras-Geometrie nicht der
  Landschaft folgt, sondern einer abweichenden Insel.
- **Ausgeschlossen (geprüft):** CPU/GPU-Terrain-Port ist 1:1 identisch
  (hash12/vnoise/fbm/rfbm/terrainH zeilenweise verglichen, gleiche
  Konstanten/Reihenfolge) → kein Noise-Port-Fehler. c_terrainH ist reine
  Weltkoordinaten-Funktion (kein Origin, kein Wrap).
- **Kandidaten:** (a) Raymarcher rendert das Terrain mit gedämpftem SDF
  (factor 0.85, fragment.glsl ~Zeile 97) → gerendertes Terrain kann von
  c_terrainH abweichen (feine/steile Strukturen) → Gras (exakt) vs.
  Raymarch (approximiert) = „abweichende Insel“. (b) Stale Halme aus
  Issue-B-Pinning (Feld folgt altem Zustand). (c) Wasser-/Steilhang-
  Grenze pro Halm vs. pro Zelle (Ufer-Sackgassen). (d) GPU-Präzision
  (highp vs. float32) — unwahrscheinlich (ulp-Große).
- **Testplan:** VM: Insel finden (Screenshot), Halme an dieser Position
  dumpen (bx,bz,d[1]) vs. c_terrainH(bx,bz) vs. Raymarch-Abgleich
  (z.B. Test-Shader oder Pixelprobe). Issue B fixen vorher — das
  entfernt Kandidat (b).

### Wahrnehmungsfrage: Fade-out beim Rückwärtsfahren
- Nutzer sieht den Death-Fade (Freeze-Test), aber beim Fahren ist er
  schwer wahrzunehmen (2s-Fade vs. 26m in 2s bei 13 m/s; nur ~1,3% der
  Halme sterben pro 1m, verteilt). Nach Issue-A/B-Fix neu bewerten;
  evtl. Death-Fade 2s → 3–4s als Tuning-Knopf.

## Session 2026-10-05, Runde 5 (Gras-Fade-out: VAO-Fix + Kanten-Pop-Fix)

### BESTÄTIGT (Nutzer-Host): Slider-Change wie geplant, Pop weg
- Slider-Change: 1s Debounce → 2s Fade-out → 2s Fade-in ✓
- Rückwärtsfahrt: plötzliches Verschwinden an der Gras-Kante weg ✓
- NEU offen: der Fade-out selbst ist beim Rückwärtsfahren nicht sichtbar
  (siehe „SOFORT NÄCHSTER SCHRITT“ oben).

### FIXED: Gras-Fade-out Teil 2 — Kanten-Pop bei Raeckwaertsfahrt
- **Symptom:** Nach dem VAO-Fix (unten) fadete der Slider-Change wie geplant,
  aber bei Raeckwaertsfahrt verschwand Gras an der Kante weiter ploetzlich.
- **Wurzel:** Zellen, die beim 1m-Shift AUSSERHALB des neuen Build-Radius
  landen (Kanten-Sichel, ~1m breit bei dCut), wurden vom Diamond-Walk nie
  besucht -> ihre Halme fielen ohne Death-Time aus dem Buffer. Gemessen
  (/tmp/test_edge_pop.c): ~3000 Halme/1m-Schritt POPTEN vs. ~11000, die
  korrekt faden. Bei 13 m/s = ~37k Halme/s am Gras-Rand = sichtbarer Pop.
- **Fix (grass_cpu.c):** Walk-Quadrat des POS-Builds = UNION von neuem
  Radius-Quadrat und altem Walk-Quadrat (old_gnx/old_gnz werden in finish
  gesichert). Die Schichel-Zellen laufen durch dieselbe Kopier-Logik
  (build_cell: outsideNew && oldIdx<0 -> return, sonst Kopieren/sterben;
  dist/half-Checks mit !partial-Guard). Selbstbegrenzt: sterbende Halme
  leben max. 2.5s (5 Builds) -> Quadrat waechst max. mit Kamera * 2.5s.
- **Verifiziert:** /tmp/test_edge_pop.c: 0 Pops pro Schritt (vorher ~3000),
  auch 2m-Spruenge/Turbo-Test (10 Schritte, 0 Pops). VM: Walk 462x464
  stabil, Fade 13119 -> 0 ueber 2.3s. Build-Dauer ohne Regression.

### FIXED: Gras-Fade-out war tot (WURZEL: glEnableVertexAttribArray(5) fehlte)
- **Symptom:** Raeckwaertsfahrt UND Slider-Change: altes Gras verschwand
  instantan, nur das neue fadete ein. Runde 4 hatte Death-Time d[10] +
  Shader-Fade eingebaut — aber visuell wirkte nichts.
- **Wurzelursache:** main.c, VAO-Setup (Zeile ~515): Attribut 5 (iDeath)
  hatte `glVertexAttribPointer` + `glVertexAttribDivisor`, aber KEIN
  `glEnableVertexAttribArray(5)`. Spec: deaktiviertes Attribut = Konstante
  (0,0,0,1) -> iDeath = 0 fuer ALLE Halme -> `if (iDeath > 0.0)` im Shader
  nie wahr -> Fade-out-Faktor nie angewendet -> Halme blieben auf voller
  Alpha, bis der naechste Build sie aus dem Buffer loeschte -> "instantan".
  Fade-in (Attribut 4, aktiviert) lief -> genau das Symptommuster.
- **Fix:** 1 Zeile: `p_glEnableVertexAttribArray(5);` vor dem Pointer-Call.
- **Wie die Fehlführung zustande kam (Lehre):** CPU, Shader-Code, VBO-
  Upload, Zeitbasis, Blending waren ALLE korrekt. Die Diagnose lief ueber:
  1) Unit-Test (Buffer-Inhalt korrekt: sterbende Halme bleiben 2.5s, Death-
     Times stimmen), 2) Instrumentierung (Taste b = grass_debug_dump():
     Zaeht lebendig/fadet/verblasst + Summe der erwarteten Shader-Fade;
     Gras-Swap-Log mit t=). Buffer zeigte 100% -> 46.9% -> 0% ueber 2s —
     aber das Auge sah nichts. Also lag es zwischen Buffer und Pixel:
     VAO-Setup zeilenweise gegen Spec geprueft -> fehlendes Enable.
- **Verifiziert (VM, Screenshot-Serie, Gras-nur-Modus, G-Kanal = Alpha-Proxy,
  identischer Test mit altem/neuem Binary):**
  - ALT: 6.28M -> 6.28M -> 6.30M -> 6.35M -> 6.30M -> 6.27M (kein Rueckgang,
    Gras voll bis zum IN-Swap)
  - NEU: 6.26M -> 6.28M -> 6.28M -> 4.80M (77%) -> 1.16M (18%) -> 1.18M
    (18%) -> 5.66M (90%) — sanfter OUT-Fade (2s), dann IN-Fade.
- **Beigehalten (Debug-Werkzeug):** Taste b = grass_debug_dump() (scene.h-
  Deklaration, grass_cpu.c Implementierung), Gras-Swap-Log mit t=.
- Hinweis: der LOD-Fade (Halme werden an der Gras-Kante mit Distanz
  kleiner, lod = 1-smoothstep(MD-12, MD, d)) ist ein ZWEITER, unabhaengiger
  Mechanismus — er maskiert den Death-Fade an der Kante teilweise, ist aber
  nicht die Ursache des Bugs (Slider-OUT faedete das GANZE Feld instantan
  aus, wo kein LOD reinwirkt).

### Test-Infrastruktur (ergänzt, /tmp)
- /tmp/test_edge_pop.c: Pop-Zaehler (alte vs. neue Buffer ueber Position,
  1m- und 2m-Schritte) — der Test, der den Kanten-Pog gefunden hat.
- /tmp/test_edge_pop_turbo.c: dito mit 2m-Spruengen (Turbo-ähnlich).
- /tmp/fade_vis2.sh: Raeckwaertsfahrt + Stopp + Screenshot-Serie
  (Fade-OUT-Visibilitaet, Gras-nur-Modus, G-Kanal = Alpha-Proxy).
- /tmp/fadeout_diag.sh: Raeckwaertsfahrt mit b-Dumps (Buffer-Fade messen).
- /tmp/fade_verify.sh: Slider-Test mit Screenshot-Serie (neues Binary).
- /tmp/fade_verify_old.sh + /tmp/64kdemo_old: identischer Test mit altem
  Binary (Vorher/Nachher-Vergleich).
- /tmp/grassfade_test.c: isolierter GL-Test (echter grass.vert + Debug-Frag
  mit vFade als Helligkeit) — fertig gebaut, GLX-Setup in der VM noch
  unfertig (BadMatch); der Bug war bis dahin per Code-Inspektion gefunden.
- /tmp/test_fadeout.c: CPU-Unit-Test Raeckwaertsfahrt (Buffer-Seite ok).

## Session 2026-10-05, Runde 4 (Fade-out, 5M, Slider-Transition, Slider-UI-Fade)

### 1) FIXED: Gras-Fade-out (Death-Time d[10]) — Mechanik drin, aber erst
Runde 5 wirklich sichtbar (Wurzel: fehlendes glEnableVertexAttribArray(5),
s. Runde 5).
- Rueckwaertsfahrt verschwand vorher ploetzlich (nb gesunken -> Halm weg).
- Jetzt: GRASS_FLOATS 11 (d[10] = Death-Time, 0 = lebt). build_cell waelt
  pro Halm: lebt+nb reicht -> kopieren; lebt+ueberzaehlig -> kopieren +
  sterben (d[10]=now); stirbt noch -> kopieren bis 2.5s; fertig -> weg.
- grass.vert: vFade = fadeIn * fadeOut (je 2s, smoothstep), cull bei <0.004
  (Clip-Ebene x=2, wie LOD-Cull). VAO: attr5 (iDeath) @ offset 40.
- Verifiziert: Unit-Test /tmp/test_death.c (6 Szenarien OK); VM-Rueckfahrt:
  "836909 Halme, 14234 neu, 22803 sterben". Log: "Gras-Swap: N, X neu,
  Y sterben (je 2s)".

### 2) CAP 5M + 3) Dichte-Slider-Kalibrierung
- GRASS_CAP 1500000 -> 5000000 (scene.h). BSS 440MB (2x 5M x 11 floats),
  VBO 220MB — auf dem Host unkritisch.
- Dichte-Slider max: 8.0x -> 5.6x (set_slider: 0.2 + pos*5.4). Kalibriert
  mit echtem Build (/tmp/cap_test.c): 5.6x + MD=120 -> exakt 5,000,000
  Halme (5.4x -> 4.97M, 6.0x -> Sattigung). "Gerade so am CAP".
- Voll-Build 5M: ~4.3s in der VM (llvmpipe); Host schneller.

### 4) FIXED: Slider-Change = eine Transition (Debounce + OUT/IN)
- War: jeder Drag-Schritt trieb einen eigenen Voll-Rebuild -> Gras
  verschwand/blendete mehrfach ein.
- Jetzt: Wert muss 1s stabil sein (Debounce), dann 2-Phasen-Transition:
  OUT-Build (Modus 1: alle alten Halme sterben, 2s Fade-out) -> 2s warten
  -> IN-Build (Modus 2: alles neu, 2s Fade-in). Position-Builds werden
  waehrend der Transition unterdrueckt. Ein Slider-Zug = eine Transition.
- Build-Modi in grass_cpu.c: 0=POS (Kopieren), 1=OUT (alte Zell-Tabelle
  durchgehen, alles sterbend kopieren), 2=IN (volle Generierung).
  OUT-Build: kein Radius-Walk, directer Tabellen-Durchlauf (~ms).
- Verifiziert (VM): ein Klick -> "827192 sterben" -> "2043554 neu".
- Performance: 1m-POS-Shift 122-136ms, 12m 299ms (VM) — Teil-Kopieren
  geblieben, kein Regression.

### 5) FIXED: Slider-UI-Fade (Maus-Naeherung/Entfernung)
- Slider+Labels blenden ein, wenn die Maus sich ueber sie bewegt (ohne
  Taste), aus, wenn die Maus 3s nicht im Bereich war. Ein 0.5s, Aus 1.5s
  (Echtzeit via rawDt, NICHT dt-Clamp — sonst 25% Tempo bei 2.5 FPS).
- Bereich = Tracks + Labels (x: winW/2-150hsc-10cs-8hsc .. winW/2+150hsc,
  y: winH-100hsc .. winH-26hsc). Ziehen (sliderDragging) = sichtbar halten.
- Labels+Tracks in eigenen Draws (Tint-Alpha = sliderFade), Status-Text
  bleibt opak. fade<=0.004: gar nicht zeichnen.
- **WURZEL des Bugs (warum es bei 4k-maximiert klappte, sonst nicht):**
  XQueryPointer! Der Prototyp in third_party/include/X11/Xlib.h:2993 ist
  eine 9-Parameter-Version (NICHT der Standard-X11-XQueryPointer):
  `(dpy, w, Window* root_return, Window* child_return, int* root_x, int*
  root_y, int* win_x, int* win_y, unsigned int* mask)` — die Root-/Child-
  FENSTER kommen VOR den Koordinaten. Der alte Code deklarierte 9 Parameter
  mit int* auf den Plätzen 3/4 (Window* wird dort geschrieben: 8 Byte in
  4-Byte-Var = latenter Stack-Overflow, harmlos geblieben) und las die
  Koordinaten von 5/6 = ROOT-Koordinaten. Maus-Look brauchte nur Deltas
  (in Root und Fenster identisch) -> Bug unsichtbar. Slider-Fade braucht
  absolute Fenster-Koordinaten -> verschoben um die Fenster-Position.
  4k-maximiert: Fenster bei (0,0) -> Root==Fenster -> Zufallstreffer.
  Fix: Deklaration + Aufruf laut Header; ptrX/ptrY = win_x/win_y (7/8).
  (Mein erster Fix-Versuch mit 8 Parametern segfaultete: child_return
  ueberlief mask, mask_return landete in nicht-übergibtem Stack.)
- Verifiziert: VM (Fenster bei 212,58): Maus weg 5s -> 38 Pixel (Terrain
  nur), Maus ueber Slider -> 12737 Slider-Pixel. Host (Nutzer): klappt.

### Test-Infrastruktur (ergänzt, /tmp)
- /tmp/test_death.c: Death-Time-Unit-Test (POS/Rueck-Shift/OUT/IN).
- /tmp/cap_test.c: Halmzaehlung fuer echte Builds (Kalibrierung).
- /tmp/qptr_test*.c, /tmp/qptr_min.c: XQueryPointer-Signatur-Tests.
- Signatur-Frage immer zuerst: third_party/include/X11/Xlib.h lesen
  (dort 9-Parameter-XQueryPointer! Standard-X11-Prototyp tauscht gegenueber
  dem Gedaechnis — Fundstelle: Xlib.h:2993).

## Session 2026-10-05, Runde 3 (Wurzelursachen: Slider-Skalierung + Gras-Fade)

### Bug 1 FIXED: Slider skalieren jetzt mit hud_scale (WURZEL: Fix war nie drin)
- **Wurzelursache:** Commit 4cbe65a "Slider+Labels skalieren mit
  Fensterhoehe" behauptete in der Message die Skalierung — der Diff enthielt
  ABER nur den Screenshot-Crash-Fix! `slW=300.0f, slH=18.0f`, `y0=h-44/72/100`,
  Label-Positionen und Hit-Test waren nie skaliert (git-Archiv geprüft: in
  KEINEM Commit). Deshalb "nicht behoben": Es war nie drin.
- **Fix (jetzt drin):** draw_sliders (slW/slH/y0/Regler-Breite ×hsc),
  Label-Positionen (150*hsc, slY×hsc, +1*hsc), ButtonPress-Hit-Test
  (sx0, Breite, y-Offsets ×hsc), MotionNotify (/300*hsc).
- **Verifiziert (VM, 1705x781, hsc=1.085):** Pixel-Messung: Tracks bei
  y=733..752/702..722/672..692, x=689..1015 (exakt wie Formel), Labels
  rechtsbuerig 12px links der Track-Kante, vertikal zentriert. Klick auf
  Track1 (800,743) triggert Voll-Rebuild (Log: 1.5M neu = CAP bei ~2.9x
  Dichte, passt zu pos=(800-689)/316). Layout ist bei JEDEM hsc eine
  uniforme Skalierung des 720p-Layouts -> keine Ueberlappung.
- Hinweis: VM-WM resize-t 1920x1080 -> 1705x781, daher max. hsc~1.085 in
  der VM testbar; auf dem Host (hsc 2..3) gilt dieselbe relative Geometrie.

### Bug 2 FIXED: Gras-Fade-in ist jetzt im sichtbaren Bereich da
- **Analyse (Gras-nur-Modus, Magenta-Hintergrund, G-Kanal = Alpha):**
  Mechanik war VOLLSTAENDIG korrekt (CPU-Birth-Times, VBO-Upload inkl.
  d[9], attr4@36, uTime/iBirth gleiche Zeitbasis CLOCK_MONOTONIC, Blend,
  weit->nah-Chunks). Kanten-Crescent fadete sauber: 0 -> 534 -> 631 px
  ueber 2s. **Problem war der UMFANG:** Nur das 1m-Crescent am weiten
  Gras-Rand (~67m bei MD=80, dCut per Bisektion auf dens=0.09) fadete ein.
  Alle anderen neuen Halme — Zellen, deren nb sich bei Kamerabewegung um
  ±1 aendert (23% der Zellen pro 1m, Simulation /tmp/nb_change.c) — wurden
  vollst.aendig regeneriert mit birth=now-10 = SOFORT sichtbar. Deshalb
  "erscheint ploetzlich".
- **Fix:** build_cell: TEIL-KOPIE. Halm-Daten sind reine Funktion von
  (ix,iz,b) (Hashes+Terrain, keine Kameradistanz) -> ueberlappende Halme
  (min(onb,nb)) kopieren (Birth-Time bleibt, Fade laeuft weiter), nur die
  Zusatz-Halme (b=onb..nb-1) neu generieren. grass_build_finish: alle
  generierten Halme (d[9]==0) blenden ein (d[9]=gWorldTime) — Crescent,
  nb-Steigerungen, Slider-Change (alles neu) und Start (Feld blendet beim
  Start ein). Der dCut-Test in finish ist damit ueberfluessig.
- **Verifiziert:** Unit-Test: 1m-Shift ~11-17k neue Halme (vorher ~1.9k),
  12m-Shift 129k. VM-Fahrt (20m, 10 Swaps): je 12-16k neu, Alpha-Messung
  (Gras-nur-Modus) steigt monoton ueber die 2s-Fade. PERFORMANCE: Teil-
  kopieren ist billiger als Regenerieren — 1m-Shift 134-280ms (alt: 298-
  479ms), 12m-Shift 320ms (alt: 1884ms).
- Verbleibend: Halme, deren nb ABNAHMT, verschwinden sofort (kein Fade-
  out; wuerde Death-Time d[10] brauchen) — hinten am Rand, meist aus dem
  Blickfeld. Falls der Nutzer es merkt: nachtraegen. [NACHGETRAGEN in
  Runde 4: Death-Time d[10] + Fade-out, s. Runde 4, Punkt 1.]

### Test-Infrastruktur (VM, /tmp)
- xhold2.c: Taste via XSendEvent halten (X11/Xtst per dlopen). WICHTIG:
  XSendEvent flaechst das Request-Buffer NICHT -> XFlush() nach dem
  KeyPress, sonst kommt KeyPress+KeyRelease zusammen an (das war der
  Grund, warum xdotool keydown in der VM nur ~1s „hielt").
- xdotool keydown --window WIN k sendet in dieser Version EIGENSTAENDIG
  KeyPress+KeyRelease (~1s auseinander) — fuer Halten xhold2 verwenden.
- X-Autorepeat staerkt's noch: xset r off bei Key-Hold-Tests.
- VM laeuft ~2-3 FPS (llvmpipe), dt-Clamp 0.1s -> Kamera max. 1.3m/Frame.
- Test-Skripte: /tmp/fade_test.sh, /tmp/fade_iso*.sh, /tmp/fade_vis.sh,
  /tmp/fade_alpha*.sh, /tmp/fade_start*.sh, /tmp/keytest.sh, /tmp/slider*.sh
- Numerik: /tmp/fade_calc.c (dCut/Crescent/Pixelgroessen), /tmp/nb_change.c
  (nb-Veränderungen pro 1m-Shift).

**Wichtig:** Nutzer testet auf dem HOST (NVIDIA), nicht in der VM.
VM-Pixel-Scans sind für visuelle Fragen nur grobe Anhaltspunkte
(WM-Resize, llvmpipe-Bugs). Bei visuellen Bugs: lieber nachfragen/
Screenshot vom Host holen als am VM-Scan festzuhalten.

### Auslieferung (wichtig, schonmal verwechselt!)

- **Ordner** `64kDemo` (grosses D) + **Executable** `64kdemo` (kleines d):
  Die Testversion liegt in `/media/sf_Quarantine/64kDemo/64kdemo`.
  `make` kopiert automatisch dorthin (Makefile-Ziel `all`). NICHT in
  ein bin-Unterverzeichnis, NICHT direkt unter Quarantine.
- Projekt-Kopie + Zip in Quarantine sind optional, Binary ist das Wichtige.

### Aktueller Code-Stand (fuer die Bug-Untersuchung)

**HUD/Slider (main.c):**
- `hud_scale(h) = max(h/720, 1.0f)` — Schrifthoehe `cs = 16*hsc`.
- Statuszeilen: y = h - 20*hsc, -20*hsc - cs, -20*hsc - 2cs (Vollstats,
  hudMode=2). Hintergrund-Quad 180*cs breit, 24*cs hoch.
- Slider: `draw_sliders()` — 3 Tracks (y = h-44/72/100*hsc, Breite
  300*hsc, Hoehe 18*hsc), Labels rechtsbuerig am Track-Linkskanten
  (x = winW/2 - 150*hsc - strlen*cs - 8*hsc). Hit-Test in ButtonPress/
  MotionNotify mit denselben Formeln.
- Font-Atlas: NUR Grossbuchstaben + Ziffern + `x / . -` (build_text_verts
  mappt a-z auf A-Z). 16x8-Zellen im 256x128-Atlas.

**Gras (grass_cpu.c + grass.vert/frag):**
- `GRASS_FLOATS=10` (d[9] = Birth-Time), `GRASS_CAP=1500000`,
  `GSTEP=0.30m`, Zellen 0.3x0.3m, Tabelle 240x240 (CT=240).
- nb-Formel: `32*dens*(0.4+0.6*h4)*grassDensityScale` (Cap 32) +
  stochastisches Runden (c_hash12(ix*0.917, iz*0.311)).
- Falloff: `pow(1 - smoothstep(8, grassMaxDist, dist), 1.0)` (LUT mit
  1024 Eintraegen, dens_at()); Build-Kante g_half() bei dens>=0.09.
- Diamond-Order: Zellen in L1-Ringen vom Zentrum (Kamera) nach aussen;
  bei CAP-Uberlauf fehlen die WEAESTEN Zellen.
- Zweipuffer + Worker-Thread; "finish then latest"; 1m-Shift-Trigger
  (~300ms auf der VM). hasOldBuild/old_bcx/old_bcz fuer Copy-Entscheidung
  und Birth-Time (d[9] != 0 = kopiert; "alt" = odist <= dCut).
- Rendering: 32 Chunks von WEIT nach NAH (glDrawArraysInstancedBaseInstance,
  GL 4.2), Chunk-Index = Distanz-Slot 31-near...; Fade im Shader:
  `age = uTime - iBirth; fade = clamp(age/2,0,1)` (smoothstep), vFade
  -> Alpha in grass.frag.
- `gWorldTime` (main.c, pro Frame = tNow) und uTime = tNow (gleiche Basis).

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

## Session 2026-10-05, Runde 2 (Skalierung, Crash, Fade-in)

- [x] **Slider+Labels skalieren mit hud_scale** — Runde 2: Commit-Message
  behauptete den Fix, Diff enthielt ihn NICHT (nur Screenshot-Fix). Runde 3:
  WIRKLICH drin (draw_sliders, Labels, Hit-Test, MotionNotify) + verifiziert
  (Pixel-Messung + Klick-Test). S. Runde 3, Bug 1.
- [x] **Screenshot-Crash (>1600px breit, VM)**: Mesa llvmpipe 26.0.8
  glReadPixels(GL_BACK, GL_RGB) schreibt (w mod 4)*(h-1) EXTRA Bytes
  ueber das Buffer-Ende (Heap-Overflow, 'malloc(): invalid size').
  Mindest-Test /tmp/readpix_test.c reproduziert es ohne die Demo.
  Fix: GL_RGBA + Alpha entfernen. (Nutzer-Feedback dazu: noch offen.)
- [x] **Gras-Fade-in** — Runde 2: nur das 1m-Crescent am weiten Rand
  fadete (deshalb "ploetzlich"). Runde 3: Teil-Kopie in build_cell, alle
  generierten Halme blenden ein (11-17k/1m statt ~1.9k), schneller Rebuild.
  S. Runde 3, Bug 2.

## Gefixt (Session 2026-10-05, Gras-Puffer + HUD)

- [x] **HUD-Text war SCHWARZ** (hud.frag seit Ausgangsstand: `c.rgb*
  uTint.rgb` mit RGB=0-Font-Textur). Jetzt `vec4(uTint.rgb, c.a*uTint.a)`.
  Darueber: Bounds-Check in build_text_verts verglich Vertex-Anzahl mit
  Zeichen-Limit -> Zeile 1 brach nach 26 Zeichen ab (AA fehlte!).
  Hintergrund-Quad jetzt vor dem Text (deckte ihn frueher ueber).
- [x] **Slider hellgrau** (Track 0.75 grau/85% statt schwarz/50%).
- [x] **Slider-Verbindungslinien (Strip-Bug)**: 3 Quads als gemeinsamer
  12-Vertex-Strip -> Verbindungsdreiecke zwischen den Quads (duenne
  Linie zwischen den Regler-Positionen). Jetzt je Quad eigener
  4-Vertex-Strip. (Der 'Faden' war ein Missverstaendnis: die Linie war
  der Bug, kein Wunsch.)
- [x] **Puffer-Verwaltung: Diamond-Order** (grass_build_step): Zellen
  vom Zentrum (Kamera) nach aussen (L1-Ringe, 4*dr Zellen/Ring).
  Bei GRASS_CAP-Ueberlauf fehlen jetzt die WEAESTEN Zellen (frueher:
  Gitter-Order von der Ecke -> naehere Zellen konnten fehlen).
- [x] **Mehr Dichte**: Basis 24->32 (Cap 32), Falloff-Exponent 1.6->1.0
  (flacher Blickwinkel in der Ferne braucht mehr Halme). 442k->802k
  Halme bei Standard. Dichte-Slider wirkt ueber den ganzen Bereich.
- [x] **Weit->nah Rendering**: 32 Chunks via glDrawArraysInstanced-
  BaseInstance (GL 4.2) vom Buffer-Ende her. Alpha-Blending (Fade-in)
  und MSAA brauchen Raster-Reihenfolge von hinten nach vorne; die
  Diamond-Order der VBO genuegt (Rebuild zentriert sie auf die Kamera).
- [x] **HUD-Vollstats (H-Taste, 3 Modi)**: Zeile 1 FPS/RES/CAM/AA
  (OFF/4x/8x)/TREES/QUADS/GRASS, Zeile 2 D/H/MD/FRZ/NUR-GRAS, Zeile 3
  POS (Kamera) + CAP (Gras-Puffer voll) + BUILD (Rebuild aktiv).
  Reduziert = Zeile 1, aus = nichts. Slider beschriftet (Sichtweite/
  Hoehe/Dichte).
- [x] **grass_build_busy()**: Worker-Status (Request wartet oder
  generiert) fuer den BUILD-Indikator.
- [x] **Kamera-Rotation pausiert waehrend Slider-Drag** (Fly-Modus).
- [x] **Slider-Change triggert Voll-Generation** (kein Zell-Kopieren):
  sprunghaftes Aendern ist gewollt (Nutzer).

## Gefixt (Session 2026-10-05, Gras-Optik-Korrektur + Farben/Nebel)

- [x] **Schritt 4 revertiert + korrigiert** (docs/grasplan.md): Der erste
  Entwurf skalierte die Halmzahl pro Zelle (Cap 12 davor) -> Luecken
  zwischen den Buescheln, dummer Nahbereich, Dichte-Regler wirkungslos.
  Jetzt: naechster Bueschel-Mittelpunkt (1.2m-Grid, 3x3-Suche pro Halm)
  mischt nur Hoehe/Yaw/Lean weich (w = 1-smoothstep(0,0.7,dc)). Position
  und Halmzahl unveraendert -> Dichte wie vorher, Regler wirkt wieder.
  1m-Rebuild ~170ms (3x3-Suche kostet ~40ms).
- [x] **Schritt 2 (abgerundete Normalen)**: grass.vert sendet vSide (ww
  -0.5..0.5) + vR (Breitenrichtung); vNrm = Flaechennormale F + Wind.
  grass.frag: n = normalize(n + 2*0.6*vSide*vR + 0.5*Y) (k=0.6, u=0.5).
  Weicher Hell-Dunkel-Verlauf quer zur Breite; keine schwarzen Halme.
- [x] **Satteres Gruen** (Gras + Terrain, per HLS-Skript): Basis
  (0.22,0.42,0.13)->(0.18,0.46,0.09), hell (0.48,0.60,0.18)->
  (0.37,0.66,0.12), Halm-Spitze (0.45,0.58,0.17)->(0.34,0.64,0.11).
  Terrain und Halm-Basis identisch -> LOD-UEbergang bleibt nahtlos.
- [x] **Nebel weniger + bläulich** (Rayleigh): Koeffizient 0.00004 ->
  0.000015 in allen 5 Shadern (250m: 92%->61%), Sky-Horizont
  (0.72,0.83,0.95)->(0.67,0.78,0.96).

## Gefixt (Session 2026-10-05, Gras-Optik + 64k-Budget)

- [x] **Gras-Optik Schritt 1 (Transluzenz)**: `pow(max(dot(-rd,SUN),0),3)`
  * (0.3+0.7*vT) * 0.55 mit Halm-Albedo — Gegenlicht, Spitze heller.
- [x] **Schritt 3 (Specular)**: Blinn-Phong, Exponent 24, Faktor 0.12,
  Spitzen staerker (dezent: Pixel = mehrere Halme).
- [x] **Schritt 4 (Clumping)**: 8m-Grid, pro Zelle Hash-Cluster-Mittelpunkt
  + Dichte-Varianz, nb * (0.21..1.0) nach Distanz zum Mittelpunkt.
  (SPÄTER REVERTIERT — Luecken/dummer Nahbereich, siehe
  "Gras-Optik-Korrektur"-Sektion; ersetzt durch 1.2m-Bueschel via
  Hoehe/Yaw/Lean.)
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

### 1. Gras (Puffer-Verwaltung + Dichte + Rendering-Order erledigt)
- [ ] Visuell verifizieren (Nutzer): Dichte naeher/ferner, CAP/BUILD-
  Indikator, HUD-Vollstats, Wide->nah-Fade-in (keine verdeckten Halme)
- [ ] Entfernungssortierung: genuegt die Chunk-Methode (32 Chunks) oder
  braucht es pro-Frame-Sortierung? (Nur waehrend Fade-in relevant —
  dann ist die Order auf die Kamera zentriert)
- [ ] Optional: Schritt 7 (Baum-Schatten auf Gras, docs/grasplan.md)
- [ ] V-Modus-Crash auf Host neu testen (maximiert + AA off)
- [ ] Gras-Flackern sehr nah an der Kamera pruefen (Compositing/Depth;
  Wide->nah-Order Koennte es bereits behoben haben)

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
- [ ] **Momentan 68,136 Byte** (Budget pausiert, bis die Demo fertig ist —
  Nutzer). Letzter unter Budget: 64,032 (Commit "64k-Budget").
  Plan steht: Linker-Script (/tmp/64k.ld) -> 66,864; .plt.sec (528B)
  discarden; REST: ~1,3KB aus main()/Strings sparen.
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
- Gras-Unit-Tests (ohne GL): `gcc -O2 -Wall -Wextra -std=c11 -Isrc -Ithird_party/include -o /tmp/test_1m /tmp/test_1m.c src/grass_cpu.c src/terrain_cpu.c -lm -lpthread` (gleiche Zeile fuer /tmp/test_fade2.c; Test-Dateien haben eigene grassDensityScale/grassHeightScale/grassMaxDist/gWorldTime-Definitionen)
- Mesa-Readback-Mindest-Test: /tmp/readpix_test.c (glReadPixels-Overflow-Check)

## Wichtige Konstanten

- `WATER_Y = 0.25`, `CAM_NEAR = 0.1`, `CAM_FAR = 300`
- Compositing-Blend-Zone: ±0.03m
- Gras: STEP 0.30m, Build-Radius = Dichte-Cutoff (~79m bei MD=80),
  Dichte-Falloff `pow(1-smoothstep(8, grassMaxDist, dist), 1.0)` (LUT),
  Halme/Zelle `32*dens` (Cap 32) — Dichte ohne Clumping-Faktor
  (Clumping mischt nur Hoehe/Yaw/Lean, CLUMP 1.2m-Grid, 3x3-Suche),
  GRASS_CAP 1.5M, GRASS_FLOATS 10
  (pos3, hoehe, breite, phase, schattierung, yaw, lean, birth)
  — hoehe/breite = BASIS-Werte, Distanz-Skalierung im Shader (grass.vert)
  — Generator im eigenen Thread ("finish then latest"), Zell-Kopieren
    macht 1m-Rebuilds ~340ms, VBO-Upload im Main-Thread
  — Zellen in Diamond-Order (Zentrum->auessen): bei CAP-Ueberlauf
    fehlen die weitesten Zellen; Rendering in 32 Chunks von weit
    nach nah (glDrawArraysInstancedBaseInstance, Blending/MSAA-Korrektur)
  — Shader: Transluzenz, abgerundete Normalen (vSide/vR, k=0.6 u=0.5),
    Specular, Grundfarbe aus Terrain (4-Oct-fbm), wandernde Boen (vnoise)
- Bäume: Rebuild >12m (camTgt), Gras: Rebuild >1m (camPos)
- Slider: Dichte 0.2–8.0x, Höhe 0.3–3.0x, Sichtweite 40–120m (Maus, unten Mitte)
- Tasten: M=AA (4x/8x/OFF), G=Gras-Freeze, V=Nur-Gras, P=Screenshot,
  T=Wireframe, O=Fly/Orbit, 1/2/3=Resolution, H=HUD (Voll/reduziert/aus),
  Q/Esc=Quit
- Caps: TREE_VERT_CAP 420000, CROWN_CAP 55000, GRASS_CAP 1500000, WIRE_CAP 90000
# 64kDemo — Status

Stand: 2026-10-08, Runde 24. **Baumgenerator ist jetzt ein Port von EZ-Tree**
(`github.com/dgreenheck/ez-tree`, MIT) — umschaltbar mit `DEMO_TREEGEN=alt|ez`
(Standard `ez`). Schattenwurf repariert (TODO 20 teilweise), ferne Kronen-Quads
repariert (15/16), Gras weiter (17), Schilf auf bestem Stand zurückgedreht (19).
Kennzahlen: Binary 129 888 Byte (Budget laut Nutzer zweitrangig), Baum-Vertices
ganze Szene **496 104** (Cap `TREE_VERT_CAP 700000`), 14 538 Canopy-Quads,
Shadow-Map 2048², 13,7 cm/Texel.
**Wichtigste Funde der Runde 24:**
1. Unser LOD (`tier = d2<121 ? 3 : d2<784 ? 2 : 0`) stammt aus der
   Ursprungscodebasis, **nicht** von EZ-Tree. Es warf ganze Ast-Level weg und
   schaltete auf eine andere Repräsentation (Atlas-Quads) → Sprung der
   Aststruktur, ferne Bäume = nackter Stamm mit hellgrünen Quads. EZ-Tree ändert
   an der Stufe **nur die Vernetzung**: `sectionStride`, `segmentFactor`,
   `leafStride`, `leafScale` (bewusst unter √2) + Hysterese 0,05, nie die
   Struktur (`tree.js:88-121`). Seit `971873e` machen wir das genauso.
2. **Hängende Zweige** waren ein eigener Port-Fehler: Kinder/Laub wurden auf den
   durch Gnarliness *gebogenen* Abschnittspfad gesetzt, die sichtbare Röhre ist
   die *Sehne* zwischen den keep-Ringen. Ansatzpunkte liegen jetzt auf den
   vernetzten Ringen (Makro `EZSEG`).
3. Referenzbilder analysiert: `fbrqdET` = Foto-Rinde, `ZVAOUSU` = **Normalmap**
   (nicht Bumpmap, Mittelfarbe (119,125,247)), `cYMdcNC` = **Blattrebe** (mehrere
   Blätter an einem Stengel, 68,8 % der Zeilen schneiden ≥3 Segmente). EZ-Tree
   **liefert Texturen mit** (11 Rinden-Sets Color/NormalGL/Roughness + Blatt-
   Texturen, Git-LFS); quicktree/Sapling nicht (nur Datei-Properties). Referenz-
   bilder im Share: `/media/sf_Quarantine/64kDemo/referenz/`.
**Als nächstes (offen, Nutzer-Feedback Runde 24):** Atlas-Quads zeigen anderen
Inhalt als unsere Einzelblätter → TODO 22 (Buschel-/Reben-Textur pro Blattfarbe)
ist der nächste Schritt; dann TODO 23 (Billboard im gröbsten LOD), 24
(Alpha-Blend-Überblendung), 25 (Schatten vom ganzen Baum, mit der neuen
Geometrie), 26 (eigener Schilf-Pass, Plan unten). Rinden-Ausbau wie in den
Referenzen (Foto-Color + Normalmap + Roughness, Tone Mapping mit Exposure) ist
vorgemerkt.
Test-Hygiene: Timeout <=120 s, nach jedem Test `pkill -9 -x 64kdemo`.
**Wichtigster Fund der Runde 23:** `DEMO_FREEZE` blendete das **gesamte Gras** aus.
`gWorldTime` (Birth-Zeit der Halme) lief in echter Laufzeit (~14 000 s), die Shader
bekamen `uTime = 3.5` -> `gf = smoothstep(0, uFadeDur, uTime - gbirth) = 0` ->
`vFade = 0` -> `if(alpha < 0.02) discard;` wirft jedes Fragment. Beweis: Gras-Frag
auf reines Magenta erzwungen = 0.00 % magenta mit Freeze, 9.83 % ohne. **Alle
Gras-Messungen mit Freeze vor diesem Fix sind wertlos.** Fix: mit Freeze
`gWorldTime = -1e6` (Fade-Animation aus).
**Als nächstes (Nutzer-Feedback aus Runde 23):** Feinast-Gerüst (Bäume wirken wie
Cheerleader, zu wenige feine Äste), Blattbuschel-Textur pro Blattfarbe für das
mittlere LOD, **ganzer Baum als Billboard im gröbsten LOD** (sonst ferne Äste ohne
Blätter), LOD-Überblendung als sauberer Alpha-Blend statt Krisselig-Discard,
Schatten: ganzer Baum statt Stumpf + höhere Auflösung, Schilf (19, läuft).
Budget ist laut Nutzer zweitrangig. Test-Hygiene: Timeout <=120 s, nach jedem Test
`pkill -9 -x 64kdemo`. Deterministische Hooks statt X-Tastendrücken: `DEMO_TREE`,
`DEMO_CAM="x z [pitch] [yaw]"`, `DEMO_FREEZE`, `DEMO_SHOT`, `DEMO_ARTEN=2`,
`DEMO_TREEMASK`, `DEMO_LEAFMASK`, `DEMO_CROWNMASK`, `DEMO_SHADOW*`,
`DEMO_GRASSMD=<m>`, `DEMO_GRASSTEX="<far0> <far1>"` (0 = aus), `DEMO_REED="<dens> <wide>"`.
**Gras braucht `DEMO_FREEZE`-Fix oder laufende Zeit** — ohne Fix ist das Gras im
Screenshot unsichtbar (siehe oben).

## TODO-Liste (Nutzer-Aufträge 2026-10-07, der Reihe nach abarbeiten)

**Reihenfolge und Status** (⬜ offen, 🔄 läuft, ✅ fertig, ⛔ blockiert):

| # | Aufgabe | Status |
|---|---|---|
| 1 | Ferne Gras-Textur wird schwarz | ✅ Runde 12 |
| 2 | Nebel weniger milchig, Sichtweite größer | ✅ Runde 13 |
| 3 | Pflanzen-Vielfalt (Höhen, Sorten, Blumen, Schilf, Regionen) | ✅ Runde 14 (Ausbaustoffen, siehe unten) |
| 4 | Baum-Stämme: offene Nähte zu | ✅ Runde 15 (Gelenk-Lücke 11 mm → 0 mm, gemessen) |
| 5 | Baum-Stämme: prozedurale Rinden-Textur | ✅ Runde 16 (Detail-Energie 0,05 → 2,11) |
| 6 | Echtes Blattwerk nah + Krone fern als Textur auf Quads | ✅ Runde 18 (Atlas 8×8, 22,9 % Deckung/Kachel) |
| 7 | Blätter im Wind | ✅ Runde 19 (Kronen-Bewegung 4,2 % → 19,0 %) |
| 8 | Schattenwurf | ⬜ |
| 9 | Himmel schöner, Wolken, Tageszeiten | ⬜ |
| 10 | **Kronen-Redesign** (Nah: nur Einzelblätter, zahlreich; weicher LOD-Übergang; Blattsilhouette statt Quadrate; keine Polygonkanten fern) | 🔶 Runde 20: Silhouette + Zweige + 137,5° + 272k Blätter (Krone 8 % → 39 %); ferne Kachel-Optik noch offen |
| 11 | Rinde auch an Abzweigungen (Oberflächenkoordinaten pro Rohr, nicht nur am Stamm) | ✅ Runde 21 (Detail 2,27 → 4,00, texturlos 57 % → 38 %) |
| 12 | Ast-Gerüst pro Baum unterschiedlich + Baumgrößen stärker streuen | ✅ Runde 20 |
| 13 | Schilf: lichter, nicht kerzengerade, Farbe wie Baumblätter, vereinzelt dicke Halme mit Kolben | ✅ Runde 21 (Anteil 59,9 % → 7,7 %) |
| 14 | Blumen auf den Wiesen | ✅ Runde 21c (Mohn 13,8 %, Lupine 14,2 % der Pflanzen-Pixel in einer Blumenfläche) |
| 15 | **Ferne Kronen-Quads: transparenter Bereich der Atlas-Textur bleibt SCHWARZ** (Nutzer, Runde 22) | ✅ Runde 23 (`a05f34a`: `if(a < thr*vFade)` ließ bei `vFade=0` alle `a=0`-Texel durch; 12 751 Pixel (5,5 %) heller) |
| 16 | **Kronen-Billboards abschalten, wenn die Kamera direkt davor steht** (sieht immer unecht aus) (Nutzer, Runde 22) | ✅ Runde 23 (`smoothstep(20,28,d)` wie beim Blatt-LOD, darunter Cull) |
| 17 | **Echtes Gras viel weiter zeichnen** — Übergang auf Boden-/Ferntextur sieht schlecht aus, Textur ist auffällig periodisch (Nutzer, Runde 22) | 🔶 Runde 23 (170 m, `CT 1400`, `DENS_P=2`: 14,37 % Bildänderung ggü. 40 m. Ferntextur gespiegelt + Kachel-Variation: 0,26 % Bildanteil, Autokorrelation ohne Periodizitäts-Peak) |
| 18 | **Baumkrone volumetrisch aufbauen** — jetzt: grobe Äste mit zig kleinen Blättern direkt daran, wirkt wie Tannennadeln, keine Volumenkrone. Gründlich recherchieren (Nutzer, Runde 22) | 🔶 Runde 23 (`fbc2599`: Buschel an Zweigspitzen, Blattfläche 10,3 % → 52,4 %. **Nutzer: zu wenig Feinast, „Cheerleader“**) |
| 19 | **Schilf: Halme zu dünn, teils zu dicht gewachsen** (Nutzer, Runde 22) | 🔶 Runde 23b — Nutzer sah sich Schilf an: „dünner gerader Halm, lange breite Blätter“. Recherche (Wikipedia *Phragmites australis*): Culm **1,5-4 m, nur 5-10 mm** dick; Blätter **18-60 cm lang, 1-3 cm breit**, oben am Halm; Kolben **15-50 cm**. Umbau: zwei Rollen pro Instanz (`vReedRole` 1=Culm 2=Blatt), Blatt-Ansatz erhöht am Culm (`p.y += reedBase*culmH`), Blatt-Taper (schmale Basis, Maximum bei 1/3), Kolben nur am Culm. Messung: Culm+Blatt-Silhouette 6,92 % (vorher 6,97 %), oberste Bildzeile 81→**74**, breitester zusammenhängender Run **104→36 px**, Runs 447→321. **Nutzer-Runde 23c:** Blätter breiter aber kürzer, Halm sichtbar (war zu dünn), dichter, Grün dunkler/satter, **und Blätter in Etagen am Halm**. Umsetzung: Culm `spW 0,42→0,95` (2,5–4,6 cm), Blatt `spH 0,75+0,35·th → 0,50+0,22·th` (kürzer), `spW 1,30+0,50 → 1,90+0,70` (breiter), `wReed 0,30→0,46`, Blatt-Ansatz quantisiert auf **4 Knoten-Etagen** (`ti = floor(tr*4)`, 0,34/0,51/0,68/0,85 der Culm-Höhe), Farben dunkler/gesättigter (Blatt `0,26/0,38/0,22 → 0,11/0,30/0,09`, Culm `0,30/0,40/0,22 → 0,16/0,33/0,12`, Boden-Tint `0,44/0,46/0,24 → 0,26/0,38/0,15`).Messung: Schilffläche 6,92 %→**17,87 %**, Run-Median 3→**4 px**, auf gemeinsamen Schilfpixeln Farbe (138,159,125)→**(124,156,110)**, Sättigung 35,8→**47,0**. **Nutzer-Runde 23d:** Grün noch dunkler/satter, Halm **nach wie vor unsichtbar**, Halmausrichtung ~vertikal mit ±10° Streuung, Pflanzenhöhe variabler, **Schilfhöhe darf nicht mit dem Gras-Slider skalieren** (eigene Einstellung, Menü später). Umsetzung: Culm-Breitenprofil von Trichter auf **Zylinder** umgebaut (`taper = max(taper, 0.95-0.85·smoothstep(0.72,1,t))` — der Gras-Trichter lief zur Spitze auf 0,06 aus, der Halm war ein unsichtbares Dreieck), `spW 0,95→1,30`, Neigung `lean = 0.02+0,20·th` (≈1–12°), pflanzenweise Höhen-Multiplikator `reedHMul = 0.55+1,05·hh` auf Culm *und* Blätter, neuer Uniform **`uReedH`** (Schilf-Höhe unabhängig von `uHeightScale`, Hook `DEMO_REEDH=`), Farben weiter dunkler/gesättigt (Blatt `0,04/0,20/0,03`, Culm `0,07/0,24/0,06`, Boden-Tint `0,12/0,28/0,09`). Messung: Run-Median 4→**5 px**, breitester Run 92→**165 px**, Sättigung 47,5→**57,4**, Farbe (128,162,115)→**(118,158,101)**; `uReedH` wirkt: 0,6 → Silhouetten-Oberkante Zeile 114, 1,6 → Zeile 0 |
| 20 | Schattenwurf: Map funktioniert rechnerisch, aber **im Bild nicht sichtbar** (Nutzer, Runde 22) | 🔶 Runde 23 (`063daad`: Blatt-Wurf lief nie ein, PCF filterte Tiefen statt Vergleiche. 22,30 % Bildänderung an/aus. **Nutzer: immer noch nur Stumpf-Schatten, mäßig aufgelöst**) |
| 21 | **Feinast-Gerüst**: zu wenige feine Äste, Krone zu klein („Cheerleader“) (Nutzer, Runde 23) | 🔶 Runde 23f — **neue Messbank: `DEMO_ONETREE=1`** (ein Baum, kein Gras) + **`DEMO_TIER=0|2|3`** (Detailstufe erzwingen) + **`DEMO_TREEDIST=<m>`** (Ganzbaum-Abstand). Damit 10–16 fps statt 1,3. Umsetzung in `trees_cpu.c`: vierte Stufe **Feinaeste** (2–4 pro Zweig, eigene Röhren 3 Segmente + eigener kleiner Buschel), Zweige pro Ast `6..16 → 9..19`, Astlänge `0,45+0,55·TH → 0,60+0,65·TH` (Krone größer), Blätter neu verteilt (Zweig-Buschel 200→120/55, Spitze 300→180/82, je Feinast 60/30). Messung (ein Baum, `DEMO_TIER=3`, 12 m Abstand): Gerüst-BBox **187→253 px** (+35 %), Gerüst-Run-Median **24,0→11,0 px** (viel feiner), Gerüst-Runs 647→862, Kronen-BBox **274→317 px**, Blattmaske 12,48 %→**14,78 %**, Baum-Vertices 2100→7716, Blätter 29 580→41 460. Ganze Szene: Vertices 580 746→586 362, Blätter 233 510→**323 027**, fps 0,77→0,74 (llvmpipe). **Optik prüft der Nutzer** |
| 27 | **EZ-Tree-Port**: Baumgenerator nach `github.com/dgreenheck/ez-tree` (MIT) 1:1 nachgebaut (Nutzerwunsch Runde 24) | 🔶 Runde 24 — neuer Generator in `trees_cpu.c` (`ez_grow`), umschaltbar mit **`DEMO_TREEGEN=alt|ez`** (Standard: ez). Portiert: Abschnitts-Ast, Gnarliness mit `1/sqrt(Radius)`, Wachstums-Kraft `strength/radius` um `cross(Astachse,up)`, Twist, geschichtete Kinder + vertauschte Radial-Slots, Radius immer aus dem Elternast, `taper` pro Level aus ihren Presets, `closeTip`. Presets: oak/ash/aspen/pine aus `*_medium.json`. **Zwei eigene Fehler gefunden und behoben:** (a) `force/radius` und `1/sqrt(radius)` sind einheitenabhängig — nach unserem `unit`-Scaling wirkten sie 10× zu stark, Krone fiel auf 26 px zusammen; Radien werden jetzt in Preset-Einheiten gerechnet. (b) `taper` war geraten (0,35) statt ihrer Werte (Eiche 0,73/0,42/0,69/0,75) — deshalb endete der Stamm dick und "abgesägt"; zusätzlich hatte ich eine da-Vinci-Flächenabrechnung erfunden, die den Stamm auf 2 cm einbrechen ließ. Die ist wieder raus, Regel ist 1:1. Messbank erweitert: **`DEMO_EZPRESET=0..3`**, **`DEMO_EZSPREAD=`**, Welt-BBox + Stammradien im Log. Messung gleicher Baum: alt 4,26×6,71 m / ez 4,54×6,90 m; alt 6,44×11,87 / ez 8,16×9,94; alt 8,48×9,10 / ez 6,97×9,36. Ganze Szene: **586 362 → 65 646 Vertices**, fps 2,48 → 22,33 (Gras aus). **Runde 24b: LOD auf ihre Semantik umgestellt + hängende Zweige behoben (`971873e`)** — Struktur in allen Stufen gleich, nur `sectionStride` (1/3/6), `segmentFactor` (1/0,75/0,4), `leafStride` (1/2), `leafScale` (1/1,25/1,3); Kinderzahl fern 85 %/60 % (wegen 646 Bäumen). Selber Baum über die Stufen: tier 3 = 4008 Vertices / 8,24×13,52×8,85 m, tier 2 = 1080 / 7,55×12,86×7,63 m, tier 0 = 306 / 5,91×9,60×7,76 m. Ganze Szene **496 104 Vertices**, 14 538 Canopy-Quads. **Offen: Atlas-Quads zeigen anderen Inhalt als die Einzelblätter (TODO 22) — Übergang nah→fern bleibt inhaltlich sichtbar. Optik prüft der Nutzer** |
| 22 | **Buschel-Textur pro Blattfarbe** fürs mittlere LOD (statt Atlas-Fremdoptik) (Nutzer, Runde 23) | ✅ Runde 25 — Atlas-Kachel = **ein Blattbuschel** (9..13 unserer Blaetter, Deckung median 27 %) statt einer ganzen Referenzkrone, **farbneutral** aufgenommen (gemittelt 241/241/241); die Blattfarbe DIESES Baumes sitzt in der Karten-Instanz (`CROWN_FLOATS` 10 -> 13) und wird in `crown.frag` multipliziert. Log belegt gleiche Farbe ueber alle Stufen, Bildmessung: 16 m (94,130,48) / 40 m (87,124,46) / 60 m (87,124,47) / 100 m (87,121,60). **Nebenbefund behoben:** `uAlbedoOnly` blieb nach der Atlas-Kapture auf 1 -> die ganze Blatt-Rendrierung lief unbeleuchtet (rohe Albedo); 16 m Blätter vorher (39,82,11), jetzt (94,130,48) |
| 23 | **Ganzer Baum als Billboard im gröbsten LOD** — sonst ferne Äste ohne Blätter (Nutzer, Runde 23) | ✅ Runde 29 — ein Kamera-Quad pro tier-0-Baum (Größe/Mitte aus den echten Laubpositionen), prozedurale Kronen-Kachel 2×2 à 128 px (Silhouette buschig ausgefranst, farbneutral, Blattfarbe als Tint), `DEMO_NOBB0=1` schaltet zurück auf Karten. Karten gesamt 545 871 → 299 700, Frame 1448 → 1331 ms, Laub in der Szene 2160 → 7606 grüne Pixel. **Optik prüft der Nutzer**; Dichtesprung an der Schaltweite 2→0 offen (siehe Runde 29) |
| 24 | **LOD-Überblendung krisselig** → sauberer Alpha-Blend (Nutzer, Runde 23) | 🔶 Runde 27 — Satz-Wechsel ueberblendet jetzt: der alte Satz loest sich aus, der neue loest sich ein (0,9 s, `DEMO_TREEFADE`), Wuerfelentscheid statt Alpha-Blend (Draw-reihenfolge-unabhaengig). Ferne Baeume, die neu in den Build-Radius kommen, blenden damit langsam ein. **Optik muss der Nutzer auf dem GPU-Host prüfen** |
| 28 | **Speicher/Datenmenge der Bäume senken** (nach Doppel-Puffer ~876 MB) | ✅ Runde 28 — Kronen-Karten instanziiert: 312 → 44 Byte pro Karte, Puffer 352 → 206 MB, Erstbuild 490 → 373 ms, Karten-Cap 4 Mio → 700 k. Bild bis auf 336 Pixel (0,15 %, Schattenkarten-Rand) unveraendert; ohne Schatten bitgleich |
| 25 | **Schatten: ganzer Baum statt Stumpf**, bessere Auflösung (Nutzer, Runde 23) | ⬜ — Ist-Zustand und Wurzel-Analyse siehe „Schatten: Ist-Zustand“ unten |
| 26 | **Eigener Schilf-Pass** (Entwurf steht fest, siehe „Plan: eigener Schilf-Pass“ unten) | ✅ Runde 31 — `reed_cpu.c` + `reed.vert/frag`: eine Instanz = eine Pflanze (Culm + 4 Blätter + Kolben, 76 Vertices, ein Draw), eigenes Grid (0,6 m-Raster, Dichte-Cull auf der CPU), Schattenwurf dabei, Gras zeichnet kein Schilf mehr. Default `DEMO_REEDPER=70` = 10 Pflanzen/m², Build 15 ms, Frame +7,6 %. **Optik prüft der Nutzer** |

## Plan: eigener Schilf-Pass (Runde 23e, vom Nutzer als Ziel festgelegt)

**Warum nötig:** eine Schilf-Instanz ist ein einziges Quad (5 Vertices TRIANGLE_STRIP).
Damit sind „Halm“ und „Blatt“ dasselbe Primitiv. Vier Anläufe (Runde 23b–23d) sind genau
daran gescheitert: Halm dicker + zylindrisch = fettes Grasblatt; Blatt am Halm hochlaufen
lassen = Grasblatt mit Knick. Eine Pflanze aus Halm **mit mehreren sitzenden Blättern** ist
mit einem Quad nicht darstellbar. Rückgängig gemacht in `466bfa4` (Stand von „deutlich besser“).

**Ziel-Architektur:**
1. **Eigener Instance-Buffer** (nicht der Gras-Buffer): pro Pflanze `pos(3), hoehe, yaw, phase, variante` —
   damit ist auch die Dichte unabhängig vom Gras-Grid und es gibt kein Gras-LOD.
2. **Eigene Geometrie pro Pflanze**, prozedural im Vertex-Shader aus einer Streifen-Vorlage:
   1 Culm-Streifen (zylindrisch, oben Kolben) + 4 Blatt-Streifen an den Knoten-Etagen,
   in **einem** `glDrawArraysInstanced` (Streifen über entartete Ecken verbunden, ~5×5 Vertices).
3. **Platzierung weiterhin über das Shader-Feld** `reedW(x,z,y) * uReedDens`, sonst stimmt die
   Pflanzung nicht mit der Boden-Tönung in `fragment.glsl` überein. Der CPU-Teil liefert nur das
   Gelände-Höhen-Filtern (0,13–0,75 m) auf einem Grid (~0,5 m) im Umkreis um die Kamera.
4. **Kein `sizeScale`** (Gras hochskaliert in der Ferne, `1 + 1,8·(1-sizeLod)`) — Schilf behält
   echte Größe, eigener Cull-Abstand, ab dem die Boden-Tönung übernimmt.
5. **Schattenwurf** direkt mit: Culm + Blätter in den Shadow-Pass (löst TODO 25 für Schilf gleich mit).
6. Später: eigene Menü-Einträge für Höhe/Dichte (Hooks existieren schon: `DEMO_REEDH`, `DEMO_REED`).

**Betroffene Dateien:** neu `src/reed_cpu.c`, `src/shaders/reed.vert`, `src/shaders/reed.frag`;
`src/main.c` (VAO/VBO, Draw, Region-Neubau, Shadow-Pass), `src/scene.h` (Caps),
`src/shaders/grass.vert` (Schilf-Zweig dort entfernen), `Makefile`.


### 2. Nebel/Sichtweite — „alles ein bisschen milchig"
- `fog = 1 - exp(-t²·0.000015)` in `fragment.glsl` (3 Stellen: Zeilen ~229, 240,
  264) und identisch in `tree.frag`, `grass.frag`, `crown.frag`, `leaf.frag`.
  Mischungs-Ziel ist `skyColor(rd)` — **auch** der helle Horizont → alles wirkt
  milchig-weiß. Gegenmaßnahmen: Koeffizient runter, Nebel-Farbe unabhängig vom
  Horizont machen (eigene Haze-Farbe), und `exp(-t·k)` statt `exp(-t²·k)`-Nähe
  prüfen, damit Nebel **erst ab großer Entfernung** kommt.
- `grassMaxDist` (Standard 110, Slider 40..150) und `CAM_FAR = 300` hochsetzen;
  Dichteprofil `DENS_R1`/`DENS_EDGEW` mitmessen (Runde 11: Randband muss mit dem
  LOD-Band in `grass.vert` identisch bleiben, sonst kommt die harte Graskante
  bei lod≈1 zurück). Baum-Cull-Entfernung und `vFade` in `crown.vert`
  (120..160 m) mitziehen.

### 3. Pflanzen-Vielfalt, botanisch sinnvoll und regional verschieden
- **Regionen:** ein weltfester Regions-Hash (z. B. `c_hash12(ix*0.011, iz*0.007)`
  auf ~150-m-Blöcken) wählt pro Region: Gras-Art(en), Grundhöhe, Farbton,
  Blumen-Anteil, Schilf-Anteil. Alles deterministisch aus (Zelle, b) — sonst
  sterben/geboren Halme bei jedem Build neu (Issue A!).
- **Unterschiedliche Grashöhen:** `hoehe` ist pro Halm im Puffer (grass_cpu.c)
  → Regionsfaktor + Höhen-Band pro Art. Wasser-/Hangabhaengig (Steilhang- und Wasser-Cull existieren schon).
- **Gras-Arten:** unterscheiden sich in Höhe, Halmbreite, Blattform (Anzahl der
  Dreiecke/Keile in `grassBase`), Biegeprofil (Lean) und Blüte. Umsetzung: eine
  „Art"-ID pro Halm (im Puffer, CPU-seitig) → Vertex-Shader wählt Profil.
- **Blumen:** Lavendel (hoher Stängel + Blütenstand, violett, eher trocken/
  sonnig), Gänseblümchen (niedrig, weiß/gelb, überall), plus existierende
  Gelb/Weiß-Punkte im Terrain (`terrainColor` in `fragment.glsl`, Zeile ~155).
  Als eigene Instanz-Klasse oder als Variante im Gras-Shader (Blüten-Quad an der
  Spitze). **Terrain-Punkte müssen mit den echten Blumen korrespondieren**,
  sonst sieht man Blumen doppelt/versetzt.
- **Schilf in Wassernähe:** Halme am Ufer (h ≈ WATER_Y..WATER_Y+0.6) hoch
  (1,5–2,5 m), breit, gelblich-grün, mit braunem Kolben. Selektion über
  `c_terrainH`-Abstand zum Wasser in `build_cell` (grass_cpu.c).
- Referenz für Wind-Animation der Halme/Blätter: GPU Gems 3, Kap. 16
  „Vegetation Procedural Animation and Shading in Crysis" (Main Bending =
  ganze Pflanze, Detail Bending = Blätter; Windvektor pro Instanz).

### 4. Offene Nähte in den Baumröhren
- **Ursache (geprüft):** `emit_tube()` in `src/trees_cpu.c:36` baut die
  orthogonale Basis pro Segment neu aus der Achse (`if(fabsf(ay)<0.9f) ... else
  ...`). Zwei aneinandergrenzende Segmente haben verschiedene Achsen → die
  Ringe am gemeinsamen Knoten haben **unterschiedliche Winkel** → Lücke/
  Überlappung = sichtbare Naht.
- **Fix:** Frame **parallel transportieren** entlang der Ast-Kette: Startbasis
  wie jetzt, dann für jedes folgende Segment `u` durch Rotation von der alten
  Achse zur neuen mitnehmen (Rotation um `cross(a0,a1)`). Ringe am Knoten sind
  dann identisch. `sides` ist für alle Segmente eines Astes gleich (6/8), also
  reicht das Mitnehmen von (u,v) pro Knoten.

### 5. Stamm-Texturen (prozedural)
- Ist ansatzweise da: `tree.frag` nutzt `atan(vPos.x, vPos.z)` + 2×`vnoise` für
  vertikale Riefen. Schwächen: `atan`-Sprung bei ±π (harte Naht), keine
  Rinden-Struktur in der Fläche, keine Ast-/Stamm-Unterscheidung, keine
  Moos-/Flecken-Variation pro Baum.
- Plan: Zylinder-UV sauber aufbauen (Winkel aus der **lokalen** Astachse, pro
  Segment stetig), fbm-gestreckt in Y (Riefen), Riss-Maske
  (`1-smoothstep` auf fbm), Alters-/Moos-Tonung über `vCol`-Seed, und für Äste
  andere Frequenz. Alles prozedural im Shader, keine Textur-Bytes.

### 6. Blattwerk
- **Nah:** echte Blätter statt prozeduraler Punkttextur — eigene Instanz-Klasse
  „Leaf": gekrümmtes Quad (2–3 Segmente), Blattform per Alpha-Test aus
  prozeduraler Masur (Mittelrippe + Blattrand), normalorientiert am Ast
  (Position/Rotung kommt von `trees_cpu.c`). Budget: Blätter nur im Nahbereich
  (< ~25 m) erzeugen, pro Baum begrenzt.
- **Fern:** Krone in eine Textur rendern und auf die bestehenden Crown-Quads
  verteilen. Vorgehen wie bei der Gras-Ferntextur (Runde 11): eigenes FBO,
  Nah-Blattwerk aus mehreren Blickrichtungen einfangen, pro Baum-Region
  cachen. **Auf die Mipmap-Falle aus Runde 12 achten.**
- Referenzen: GPU Gems 3 Kap. 16 (Crysis: Leaf-Cards + Distant Sprite
  Generation), Crytek/„vegetation LOD".

### 7. Blätter im Wind
- Im Vertex-Shader der Blätter: Main Bending (Ast-Biegung, existiert schon als
  `sway0/sway1` in `trees_cpu.c` d[9]) + Detail Bending (pro Blatt-Phase
  fluttern, Amplitude ∝ Abstand zur Blattbasis). Windvektor global aus `uTime`
  + `vnoise`, Böen wie in `grass.frag` (wanderndes Koordinatensystem).

### 8. Schattenwurf
- Ziel: Baum-Schatten auf Terrain **und** Gras, Gras-Schatten angedeutet.
- Weg für diese Hybrid-Pipeline: **ein Richtungsschatten (Sonne) als Depth-Map**
  (Richtungs-Ortho von der Sonne, Bäume + Gras als Depth), dann im Raymarch
  (`fragment.glsl`) am Treepunkt samplen und in `tree/grass/crown/leaf.frag`
  ebenfalls (World→Shadow-UV). PCF 3×3 oder 4×4 gegen Kantenflimmern.
- Billig-Variante als Zwischenschritt: Baum-Schatten analytisch als
  „Zylinder-Nähe"-Test im Terrain-SDF (nur Stämme + Hauptäste, grobe Radien).
- Nach dem Schatten: `uFarFade`/LOD-Bänder neu abstimmen (Schatten machen die
  Gras-Kante sichtbarer).

### 9. Himmel, Wolken, Tageszeiten
- `skyColor(rd)` ist in 5 Shadern dupliziert (fragment/tree/grass/crown/leaf) —
  vor Tageszeiten **erst zentralisieren** (gemeinsamer GLSL-String, z. B. über
  `tools/gen_shaders.sh`-Concat), sonst fünf Orte zum Pflegen.
- Sonne `const vec3 SUN = normalize(vec3(0.55,0.38,-0.62))` → Uniform
  `uSunDir` + `uSunColor`, aus einer Tageszeit-Uniform abgeleitet (Höhe/Azimut).
  Himmelsmodell: Rayleigh/Mie-Ansatz (Preetham/Hosek sind die Referenzen; für
  64k eher handgetunter Farbverlauf + Sonnen-Scheibe + Dämmerungs-Rotanteil).
- Wolken: existieren als 2D-fbm im Sky-Shader. Besser: eigene Wolken-Schicht im
  Raymarch (Höhenschicht, fbm-Dichte, Selbstverschattung, Sonnen-Glow) —
  kostet Laufzeit, keine Bytes.
- Tageszeit beeinflusst: `SUN` in allen Shadern, Nebelfarbe, Terrain- und
  Gras-Beleuchtung, Wasser-Reflexion.

### 10. Dauerhaft offen
- **Fix nach `main` bringen** (Branch `dbg-gras-fade`, jetzt 13 Commits vor
  `main` = `666de2c`). Nur die Sachteile: Fade-Gruppen + Intervall-Markierung,
  A2C-ohne-Blending, neues Dichteprofil, Winkeldünnung, Ferntextur + Mipmap-Fix.
  Debug-Schalter C/J/K/L/Y/I/U/Z, Farbmodus und Debug-Dumps bleiben draußen.
- **Wahrnehmung:** 1 s Fade ist laut Nutzer gut.
- Budget zurück unter 64 KB (zweitrangig).
- Optional: vorbestehende Hash-Kollision (Zelle 83,9, b=11 vs b=12) fixen —
  2 Halme von ~850k, im Original schon vorhanden.

## Schatten: Ist-Zustand (Runde 23e, gemessen — Antwort auf Nutzer-Frage)

**Werfen** (in die Map gezeichnet, `src/main.c` Zeile ~1857): Baum-Mesh (Stamm + Äste,
`progTMesh`, Welt-Mesh pro Region), Kronen-Quads (`progCrown`, im Wurf-Pass senkrecht zur
Sonne via `uLightFace=1`), Einzelblätter (`progLeaf`, `DEMO_SHADOWLEAF=1` ist Standard).
**Gras und Gelände werfen nichts** — im Wurf-Pass gibt es keinen Grass-/Terrain-Draw.

**Empfangen** (samplen `shadowAt`): alle fünf Shader — `fragment.glsl` (Boden), `tree.frag`,
`grass.frag`, `crown.frag`, `leaf.frag`. **Gras empfängt also Schatten**, wirft aber keinen.

**Map-Parameter:** 2048², ±45 m um die Kamera → 13,7 cm/Texel, Bias 6,6 cm, 5×5-PCF.

**Messung** (Kamera 31,9 | 0,78 | 28,0; Baum bei 31,91 | 34,17, 6,2 m voraus, 3,69 m hoch;
Shadow-Map per `DEMO_SHADOWDUMP` exportiert und Weltkoordinaten rekonstruiert):

| Befund | Zahl |
|---|---|
| Map gefüllt | 18,3 % |
| Beitrag der **Blätter** zur Map | **0,68 %** der Texel (28 509 von 4,2 M) |
| Map am Lichtraum-Platz **dieses Baums**, Fenster 5,6 m | **0 % gefüllt — der Baum wirft nicht** |
| Werfer-Entfernung zur Kamera | 0–10 m: 13 867 Texel · 20–30 m: 162 835 · 50–60 m: 106 164 |
| Bodenraster 41×41 m um den Baum beschattet | **9 von 1681** Punkten |

**Schluss:** der Baum direkt vor der Kamera erscheint gar nicht in der Map, während Bäume in
10–60 m Werfer sind. Die sichtbaren Schatten gehören also zu **anderen** Bäumen — das erklärt
beide Nutzer-Beobachtungen exakt: „nur simple Stumpf-Schatten“ **und** „Schattenwurf passt nicht
zum Sonnenstand“. Die Sonnenrichtung selbst ist konsistent: `SUN = normalize(0.55,0.38,-0.62)`
in allen fünf Shadern, der Wurf-Pass nutzt denselben Vektor (verifiziert per Code-Abgleich).

**Noch nicht belegt:** warum der nahe Baum fehlt. Ausgeschlossen: Baum-Mesh enthält ihn
(Sortierung nah→fern, `TREE_VERT_CAP 700000` bei 590 658 Vertex — Cap nicht erreicht),
`tree.vert` cullt nicht nach Distanz, `crown.vert`-Cull ist im Wurf-Pass deaktiviert,
Blatt-Cull läuft über `uLodCam`. Nächster Test: `DEMO_SHADOWVIS`-Modi an einem Punkt direkt am
Baum und Map-Dump mit/ohne Baum-Mesh-Draw.

## ⚡ SOFORT NÄCHSTER SCHRITT (nächster Chat)

**Punkt 2 der TODO-Liste: Nebel/Sichtweite.** Dann 3, 4, 5 … (Tabelle oben).

**Nach dem Ferntextur-Fix noch abzustimmen:** Übergabeband (25..45 m) und
Blickwinkel-Dünnung (U) schneiden sich — wenn die Halme bei 45 m ausdünnen, muss
die Textur längst voll da sein. Zusammen mit der Nebel-Änderung probieren.

**Weitere offene Punkte:**
(siehe TODO-Tabelle oben)

## ISSUES (Nutzer-Beobachtungen 2026-10-05, nach Pop-Fix)

### R-Reset: Gras wird nicht mit zurueckgesetzt — **FIXED (VM-verifiziert)**
- **Beobachtung (Nutzer):** R setzt nur die Kamera zurueck, das Gras
  bleibt am alten Build-Zentrum (bzw. bei Freeze gar kein Build).
- **Fix (main.c):** R-Handler ruft nach der Kamera-Reset auch
  grass_build_request(30, 34, 0) auf (gCX/gCZ auf File-Scope geholt,
  damit der automatische Trigger nicht noch ein zweites Build feuert).
  Funktioniert auch im Frozen-Zustand (Worker arbeitet explizite
  Requests ab, Abholung/Upload ist frozen-unabhaengig).
- **Verifiziert (VM):** 10s fahren + R: Build an (30,34), 159k neu
  (Fade-in), 220k alt (Fade-out), sichtbar 839731 ≈ Start 827192.
  Frozen: 10s fahren + R: 827192 Halme, 0 neu, 532 sterben (alles
  kopiert, 100% sichtbar).

### Issue A: „Neue Halme ploppen auf“ — **GELÖST (Alpha-to-Coverage, Nutzer-Host-Test)**

**Ursache (Runde 10):** halbtransparente sterbende Halme schreiben Tiefe und
verdecken eingespielte Halme dahinter; beim harten Cull (`f < 0.004`) fallen die
Tiefen weg und die verdeckten Halme werden schlagartig sichtbar. Gemessen:
55,8 % der eingespielten Halme im Bild liegen hinter einem näheren sterbenden
(`/tmp/test_zhide.c`). Fix: `GL_SAMPLE_ALPHA_TO_COVERAGE` im Gras-Pass, dabei
`GL_BLEND` aus (sonst doppelte Dämpfung). Sortierte Zeichenreihenfolge bringt
daneben nichts Sichtbares mehr.

Die folgenden drei Teile waren Umwege über die Daten-Seite — die Daten waren
die ganze Zeit sauber. Teil 1 war ein echter Bug, Teil 2 und 3 auch, aber nicht
die Ursache des sichtbaren Sprungs.

**Teil 1 — FIXED (committet a4ddeb9, VM-verifiziert):** Death-Time wurde
während des Builds gesetzt → sterbende Halme waren beim Swap schon
0.3–1s im Fade → sofortiger 15–30%-Sprung am Swap-Punkt. Fix: Death-Time
erst am Finish (Swap-Punkt) setzen (Platzhalter -1.0f).

**Teil 2 — FIXED (committet, VM-verifiziert): Overlap-Bug im Copy/Gen-Pfad.**
- **Wurzel (zwei Mechanismen, per exaktem Float-Vergleich belegt):**
  1. **onb-Off-by-one:** onb (altes nb) wurde im Copy-Pfad am
     ZELLENZENTRUM (x,z) berechnet, aber der alte Build generierte am
     PATCH-ZENTRUM (px,pz, bis 0.106m Offset) → LUT-Bin + stochastisches
     Runden → onb weicht vom echten alten nb um ±1 ab → generierter Halm
     bei b=onb dupliziert den kopierten Halm. (Bereits nach EINEM
     Dichte-Wechsel: 331 Duplikate.)
  2. **Sterbende-Halme-Overlap:** Bei Dichte-Abfall werden Halme
     b=nb..ocnt-1 sterbend; steigen sie innerhalb des 2.5s-Fade-Fensters
     wieder, werden sie kopiert (Fade läuft) UND ab bStart=onb neu
     generiert → Overlap. (Mit dichten Builds: 63.790 Duplikate.)
- **Fix (grass_cpu.c):** b-Index pro Halm speichern (d[11], GRASS_FLOATS
  11→12, CPU-nur, Shader liest ihn nicht, VAO-Stride automatisch). Copy-
  Loop baut eine 32-Bit-Maske `copiedMask` der tatsächlich kopierten b's;
  Gen-Loop erzeugt nur b < nb, die NICHT kopiert wurden (statt bStart=onb).
  onb-Berechnung + old_bcx/old_bcz entfernt (wurde überflüssig).
- **Verifiziert (Unit-Test /tmp/test_overlap4.c, exakter Float-Vergleich,
  3-Phasen-Szenario (0,0)→(−10,0)→(10,0)):**
  - Vorher: Phase 2 = 331, Phase 3 = 983 Duplikate (3s-Abstand);
    Phase 3 = 63.790 (0.3s-Abstand, Fade läuft).
  - Nachher: Phase 2 = 1, Phase 3 = 1 (beide Szenarien) — nur noch die
    vorbestehende Hash-Kollision (s. u.).
  - Regressionstests: test_edge_pop (0 Pops), test_death (6 Szenarien OK),
    test_1m (1m ~140ms, 12m ~311ms, wie vorher). Binary-Größe unverändert
    (72.248 B). VM-Pixeltest Hin-und-Her-Fahrt: G-Kanal + Summe-ShaderFade
    identisch zum Original-Binary (kein Regression).
- **NEU Befund (vorbestehend, eigenständig): Hash-Kollision.** Zelle (83,9):
  b=11 und b=12 liefern exakt dasselbe (bx,bz) (doppelte Hash-Kollision
  c_hash12). 1 Halm von 587k doppelt, im Original-Code schon da, visuell
  vernachlässigbar (perfektes Überlappen). Fix = Hash-Funktion ändern
  (betrifft alle Halme) — optional, eigener Schritt.
- **Test-Artefakte:** /tmp/test_overlap4.c (exakter Float-Vergleich,
  3-Phasen + 0.3s/3s-Szenarien), /tmp/test_phase1.c (Phase-1-Duplikate),
  /tmp/test_dupdetail.c (b-Indizes der Duplikate), /tmp/grass_cpu_orig.c
  (Original für Vorher/Nachher), /tmp/issueA2.sh + /tmp/issueA2_orig.sh
  (VM-Pixeltest Hin-und-Her, G-Kanal).
- **Nächster Schritt:** Nutzer-Host-Test (Hin-und-Her-Fahrt) → Sprung weg?

**Freeze-Szenario (Freeze ON → Fahrt → Freeze OFF = POS-Build nach großem
Kamerahop) separat verifiziert (Runde 6, VM + Unit-Tests):**
- /tmp/test_freeze_pop.c (exakter Float-Vergleich): **0 Pops** bei 1/7/17/34/57 m
  Kamerahop — jeder Halm mit Fade≈1 im neuen Buffer hat einen Halm mit Fade≈1
  an EXAKT derselben Stelle im alten Buffer.
- /tmp/test_gone.c: **0 Halme verschwinden ohne Fade** — alle 827192 voll
  sichtbaren alten Halme sind im neuen Buffer wieder da (lebendig oder sterbend).
- /tmp/test_visjump.c (summierte sichtbare Alpha lod·fade um die Kamera,
  view-unabhängig): Übergang **wächst sanft über 2s** (+2/+5/+8/+10 % bei 20 m,
  +3/+11/+18/+21 % bei 42 m) — kein Sprung in einem Frame.
- VM (Summe-ShaderFade, b-Dump, /tmp/frz2.sh): 100,0 % → Swap → 87,7 % →
  90,5 % → 91,9 % (sanft). Swap-Log: 959831 Halme, 132639 neu, 77775 sterben.
- **WICHTIG: G-Kanal-Screenshots sind als Pop-Messung UNBRAUCHBAR.** Der Shader
  skaliert die Halmgröße mit Kameradistanz (sizeScale = 1+1,8·(1−dens), bis
  2,8×) — reine Kamerabewegung ändert die bedeckte Pixelmenge um +50 %.
  Außerdem ist lod NICHT in der Alpha (vFade = f), sondern skaliert nur die
  HalmHÖHE: ferne Halme sind flache, volle-Alpha-Streifen am Boden.
- **Offene Frage an Nutzer:** Wo genau ist der Sprung sichtbar (Nahbereich /
  Gras-Kante in der Ferne / flächig)? Kandidat ist die harte Gras-Kante:
  Generierung schneidet bei dens<0,09 ab (Build-Radius ~65 m bei MD=80), die
  Dichte fällt dort auf ~1–3 Halme/Zelle und dann abrupt auf 0. Springt das
  Build-Zentrum (Freeze-Hop), wandert diese Kante und ein Band neu generierter
  Halme fadet innerhalb 2 s ein.

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

### Issue C: Inseln, wo das Gras einer „abweichenden Insel“ folgt — **erledigt (99 %)**
- **Status 2026-10-06:** Nutzer hat es **ein einziges Mal** gesehen und seit dem
  Issue-B-Fix nicht mehr. Auf Nutzerentscheidung als erledigt geschlossen,
  ohne Ursache. Der untenstehende Testplan bleibt stehen, falls es wieder
  auftritt — Kandidat (b) war mit hoher Wahrscheinlichkeit die Ursache:
  Stale Halme aus dem Issue-B-Pinning, und das ist fixed.
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

## Session 2026-10-06, Runde 7 (Issue A Teil 3: harte Gras-Kante)

### WURZEL GEFUNDEN: harte Dichte-Kante liegt VOLLHALB des Shader-LOD-Fades
Nutzer-Eingrenzung: der Sprung ist bei kleiner Rueckwaertsfahrt (~4m) **nur in
der Naehe der Graskante**, bei langer Fahrt fast ueberall; Ausloeser ist das
Fade-out nach Freeze-AUS; die Laenge aendert nur die Staerke.

Messung (/tmp/test_edgeband.c, radiale Histogramme relativ zur Kamera, Hop 4m):
- Neu generierte (fade-in) Halme haeuufen sich bei **55–70m** (6065/6381/3436
  pro 5m-Ring), waehrend im Nahbereich 5–15m nur 80–619 entstehen.
- Dort ist die Shader-Hoehe **lod = 1,00** (volle Halmhoehe), d.h. die Kante ist
  ungedaempft sichtbar.
- Grund: `g_half()` und der Generierungs-Fruehabbruch schnitten beide bei
  **dens < 0.09** ab. Die CPU-Dichte faellt zwar glatt auf 0 bei grassMaxDist,
  aber der Abschnitt kommt frueher: Kante bei **66,7m** mit 2,02 Halmen/Zelle,
  dann abrupt 0. Der Shader-LOD-Fade laeuft aber erst **grassMaxDist-12 ..
  grassMaxDist = 68..80m** — die Kante liegt also INNERHALB des ungedaempften
  Bereichs. Wandert das Build-Zentrum, wandert die Kante und ein Δ-breites Band
  vollhoher Halme fadet ein = wahrgenommener Sprung.
- WICHTIG: die Halme werden **ordnungsgemaess eingeblendet** (Geburtszeit =
  Swap-Zeit, 2s). Das Einblenden selbst ist das Sichtbare, weil es an der
  ungedaempften Kante passiert.

### FIXED: `GRASS_DENS_CUT = 0.01f` (vorher hartkodiert 0.09f an zwei Stellen)
`src/grass_cpu.c`: benannte Konstante eingefuehrt und an allen drei
Dichte-Schwellen benutzt (Bisektion `g_half()`, Kopierpfad `dens >= ...`,
Generierungsabbruch `dens < ... && !partial`). Damit liegt die Kante bei
**~75,8m mit lod = 0,29** (Abschnitt bei 0.01) statt 66,7m mit lod = 1,00.
Vorher/Nachher (Dichte des statischen Feldes, Halme/m²):
- vorher: 13,0 (60–65m) → 4,3 (65–70m) → 0,15 (70–75m) → 0   (harter Schnitt bei lod≈0,7–0,9)
- nachher: 14,0 → 8,1 → 2,9 → 0,16 → 0                      (harter Schnitt bei lod≈0,11)
Der harte Schnitt ist damit ~9x weniger sichtbar und das Feld faellt ueber
65–80m weich durch den LOD-Fade aus.

### Kosten (gemessen, git-stash-Vergleich)
- Initialer Build 863 → 952 ms (+10%), 1m-Shift 141 → 172 ms (+22%),
  Gross-Shift 301 → 351 ms (+17%)
- Halme 828129 → 850340 (+2,7%); Binary-groesse unveraendert 72248 B
- Regressionen: test_edge_pop 0 POPTEN, test_death OK, test_freeze_pop 0 Pops
  (1/7/17/34/57m), test_gone 0 ohne Fade, VM: Summe-ShaderFade 100 → 87,9 →
  90,5 → 91,4% (sanft)

### Nicht behoben (bewusst)
Die Dichte-Profile haengen am Build-Zentrum, nicht an der Kamera. Bei einem
GROSSEN Zentrumshop aendert sich die Dichte auch im Mittelfeld (4–6k neue
Halme pro 5m-Ring bei 4m Hop, lod=1,0). Bei normaler Fahrt (1m-Schritte) ist
das pro Build klein und laut Nutzer nicht sichtbar; bei langem Freeze bleibt
ein weicher, aber deutlicher Dichte-Aufbau ueber 2s. Wer das auch noch will:
Dichte-Profil weltfest machen oder Build-Zentrum nach dem Freeze-AUS
interpolieren (hoch prioritaer, noch nicht gemacht).

## Uebergabe an die naechste Session (Stand nach Runde 10)

**Branch:** `dbg-gras-fade`, 8 Commits vor `main` (`666de2c`). Arbeitsbaum sauber.
Binary 76.416 Byte — der Debug-Zweig ist NICHT budget-konform (64k-Ziel), er ist
nur zum Testen auf dem Host.

**Issue A ist geloest.** Nutzerbefund: mit A2C kein Sprung, keine sichtbare
Abstufung. Sortierte Zeichenreihenfolge: Effekt 'mild bis unsichtbar'.
**Entscheidung: A2C genuegt, Sortierung kommt nicht in main.**

**Was von diesem Zweig nach `main` gehoert (minimaler Fix):**
1. `grass.vert`: `iGrp` + `uniform vec2 uFadeGrp[NGRP]` + Fade ueber Gruppen
   (nur zusammen mit dem CPU-Teil aus e5249ef moeglich — Layoutaenderung).
2. `main.c`: `GL_SAMPLE_ALPHA_TO_COVERAGE` im Gras-Pass AN, `GL_BLEND` dann AUS
   (und umgekehrt), nach dem Pass wieder disablen. Plus `p_glUniform2fv` + `locGrp`.
3. `grass_cpu.c` + `scene.h`: Fade-Gruppen, Intervall-Markierung, `cellNextB`,
   `GRASS_FLOATS` 12->10, `GRASS_NGRP` 512.
**Was NICHT nach main gehoert:** C/J/K/L/Y/I-Schalter, Farbmodus, Debug-Dumps,
`dbgGrassDepth`, sortierte Walk-Ordnung. Slider-Hide bei Maushalt ist gewollt,
aber separat gegenpruefen (in der VM nicht testbar).

**Offene Punkte (grobe Reihenfolge):**
- Issue C: Gras-Inseln — **erledigt (99 %)**, nur 1x gesehen, seit Issue-B-Fix nie wieder.
- Wahrnehmung: sind 1 s Fade lang genug?
- Dichte, Halmbreite, Farben, Baum-Schatten.
- Budget: zurueck unter 64 KB. Der Gruppen-Umbau bringt -16 % Puffer und
  -17 % Upload; das ist der Hauptgewinn, aber der Debug-Zweig muss erst
  abgespeckt werden.
- `src/shaders_gen.c` ist generiert (.gitignore) — nie von Hand editieren.

**Testbasis (alles /tmp, laeuft ohne Grafikausgabe):**
- `test_grp.c` Duplikate / Pops / Fade-Bilanz / Gruppenbelegung
- `test_idem.c` Idempotenz, `test_there.c` Hin-und-zurueck (kein Leck)
- `test_growth.c` alive/dying, `test_grplive.c` Gruppen bei 1s/30s
- `test_zhide.c` **der Beweis fuer Issue A**: 55,8 % der eingespielten Halme
  liegen hinter einem naeheren sterbenden
- `test_order3.c` Sortierqualitaet (31,77 m -> 0,12 m Abweichung)
- `timing.c` alt vs. neu (157 -> 139 ms/Build, 43,8 -> 36,6 MB)
- `/tmp/oldver/` alter Code fuer Vergleiche, `/tmp/backup_runde9/` vor Refactor

**Messfallen, die wir schon hatten (nicht nochmal reinlaufen):**
- G-Kanal-Screenshots sind als Pop-Mass wertlos: das Shader skaliert Halmgroesse
  mit Kameradistanz (`sizeScale`), reine Bewegung aendert die bedeckten Pixel um ~50 %.
- 'Falsche Nachbarpaare' (~50 %) ist kein brauchbares Ordnungsmass — innerhalb
  eines Slabs egal. Aussagekraeftig ist die Abweichung von der idealen Ordnung.
- VM laeuft mit 1-2 FPS und llvmpipe: visuelle Beurteilung unbrauchbar, nur
  CPU-Tests und Host-Augen zaehlen.
- `xdotool mousedown` erreicht das Demo-Fenster in der VM nicht -> Maus-UI nicht
  automatisierbar.

## Recherche: Blender „Sapling Tree Gen“ (Add-on-Quelle v0.3.7, 2026-10-07)

Quelle: Extension-Registry `extensions.blender.org` -> `sapling_tree_gen` v0.3.7,
`utils.py` (2003 Zeilen) + 9 Baum-Presets. Gefunden über die Registry-API
(`https://extensions.blender.org/api/v1/extensions/?search=tree`); die GitHub-Pfade
der alten Add-on-Datei sind tot (404), das Add-on ist seit Blender 4.2 eine Extension.

**Was Sapling anders macht als unser jetziger Baum — und was wir übernehmen:**

1. **Drei Hierarchie-Ebenen** (`levels = 3`): Stamm -> Aeste -> **Zweige**.
   `branches = (0, 60, 30, 10)` pro Ebene, `length = (1.0, 0.33, 0.375, 0.45)` relativ,
   `curve = (0, -30, -25)` (Aeste biegen erst nach unten, dann wieder hoch),
   `attractUp`/`attractOut` steuern Wuchsrichtung. -> Unser Baum hat nur EINE Astebene.
2. **Blaetter sitzen auf den Zweigen, nicht in einem Kronenvolumen.**
   `childStems = leaves * (0.1 + 0.9*(branchL/maxbL)) * shapeRatio(leafDist, 1-offset)`:
   Anzahl pro Zweig skaliert mit der relativen Zweiglaenge und einer Verteilungs-
   kurve entlang des Mutterasts. -> Genau das fehlt: unsere Blätter sind Zufallspunkte
   in einem Ellipsoid, deshalb „vereinzelt begrünte Zweige“.
3. **Blattstellung = golden angle**: alle Presets nutzen `leafRotate = 137.5°`
   (auch `rotate` der Astebenen). Aufeinanderfolgende Blätter werden um 137,5°
   versetzt -> gleichmaessige Besetzung ohne Klumpen/Abschirmung. Billig und grosser
   Effekt.
4. **Blatt-Orientierung**: `leafDownAngle = 45°` (Variation `leafDownAngleV = 10°`,
   und `downV = -downAngleV * offset`: blaetter zur Zweigspitze liegen flacher am Ast),
   `horzLeaves`, `bend` (Verdrehung abhaengig vom Azimut, damit ein haengendes Blatt
   immer nach unten haengt).
5. **Blatt-Form**: `leafScaleX = 0.2…0.65` — Blätter sind **in die Laenge gestreckt,
   nie quadratisch**. `leafScaleT` = Skalierung entlang des Zweigs (zur Spitze kleiner),
   `leafScaleV` = Zufallsstreuung. Silhouette kommt aus der Textur (Alpha), nicht aus
   der Geometrie.
6. **Blattgroessen real**: `leafScale = 0.18…0.25` der Baumhoehen (Birke 12 m ->
   0,2 -> Blatt ~24 cm inkl. Stiel, reales Blatt 5-8 cm).
7. **Blattmengen real**: Birke 30*10 Zweige * 16 Blätter = 4.800; Weide 35*15 * 150 =
   **78.750**. Die 100.000 des Nutzers sind also fuer grosse Bäume realistisch.
8. **Kronenform = Astlaengen-Profil entlang des Stamms** (`shape` 0…10, z.B.
   `0.5 + 0.5*ratio/0.7` dann abfallend). Verschiedene Arten = verschiedene Profile,
   nicht verschiedene Grundgeometrien.
9. **Wind**: pro Bone ein Noise-Modifier (`gustF = 0.075`), Blätter bekommen eigene
   Bones -> hierarchisches Schwingen (Ast schwingt, Blatt flattert darauf). Bestaetigt
   unseren Ansatz aus Runde 19.

**Umsetzung fuer uns (TODO 10/11/12):** dreistufige Ast-Hierarchie pro Baum individuell
(geseed), Blätter entlang der Zweige mit 137,5°-Phyllotaxis, Blattsilhouette per
Alpha-Test im Shader, Blatt-Laenge/Breite-Verhaeltnis ~2:1, Blattgroessen-Abstufung
zum Zweigende hin, Kronenform ueber das Astlaengen-Profil.

## Session 2026-10-08, Runde 27 (LOD-Ueberblendung)

**Nutzer:** "wenn der LOD, den er gerade braucht, noch nicht vorliegt, sollte der
andere nicht schon ausgeblendet werden — die Baeume springen ziemlich wild zwischen
den LODs", "auch die Baeume ganz in der Ferne sollten sich langsam einblenden,
aehnlich wie beim Gras".

Nach dem Upload bleibt der bisher gezeichnete Satz erhalten und loest sich aus,
waehrend der neue sich einloest (`uTreeFade`, Default 0,9 s, `DEMO_TREEFADE`).
Kein Alpha-Blend, sondern Wuerfelentscheid — unabhaengig von der Draw-Reihenfolge:
Baum-Mesh loest sich pro Weltwuerfel auf (tree.frag), Karten ueber vFade
(crown.vert), Einzelblaetter pro Blatt (leaf.vert keep-Faktor). Schatten-Wurf nutzt
nur den neuen Satz (stabile Schattenkarte). Messbank: `DEMO_TREEDEBUG`,
`DEMO_TIERSWITCH=<s>`, `DEMO_TREEBUILDRANGE`.
Gemessen: Faktoren laufen 0.00 -> 0.63 -> 1.00 mit Summe 1 (Protokoll der
Satz-Wechsel). Visuelle Beurteilung steht auf dem GPU-Host aus — in der VM laeuft
die Ueberblendung wegen 1,4 s/Frame innerhalb eines Frames ab.

## A/B gemessen: Stand `b73bee5` (Worker-Thread) gegen Stand nach Runde 30

Gleiche Szene (648 Bäume), Software-GL in der VM, 640×360, `DEMO_NOTERRAIN=1`:

| | `b73bee5` | jetzt |
|---|---|---|
| Karten-Datensatz | 3 275 226 Eckpunkt-Instanzen | **299 904 Instanzen** |
| Puffer pro Satz | 352 MB | **195 MB** |
| Upload beim Erstbuild | 141 ms | **78 ms** |
| Erstbuild gesamt | 469 ms | 380 ms |
| Frame-Median Blick zum Wald | 1354 ms | 1347 ms |
| Frame-Median Fernblick | 1191 ms | 1203 ms |

Frame-Zeit gleich: die eingesparten Vertex-/Uploadkosten sind in die **Full-Rate**
der jetzt vollen fernen Kronen gewandert. Sichtbar: ferne Kronen sind gefüllt statt
dünn (Laubpixel in der Szene 2160 → 7606).

## Session 2026-10-09, Runde 31 (Eigener Schilf-Pass, TODO 26)

**Umsetzung nach dem Plan aus Runde 23e:** Schilf läuft nicht mehr als Gras-Quad.

- **Neu** `src/reed_cpu.c`: eine Instanz = **eine Pflanze** — `pos(3), hfac, wfac,
  yaw, phase, variante` (8 Float). Gelaende wird pro Zelle (0,6 m) **einmal**
  abgetastet (`c_terrainH` kostet 0,76 µs; ein Raster pro Pflanze wären 160 000
  Abtastungen = 120 ms), die Pflanzen liegen gestreut darin. Der Dichte-Cull
  (Feld `reedW` × 0,46 × Dichte gegen `c_hash12`) läuft **auf der CPU** mit —
  ohne hätte die CPU 41 823 Instanzen gebaut, von denen der Shader 38 122 wieder
  verworfen hätte (gemessen).
- **Neu** `src/shaders/reed.vert` / `reed.frag`: eine Pflanze = **5 Streifen**
  (Culm + 4 Blaetter an Knoten-Etagen) à 6 Stationen, mit entarteten Ecken an
  einem `TRIANGLE_STRIP` = **76 Vertices**, **ein** `glDrawArraysInstanced`.
  Culm kamera-/sonnenzugewandt (3 cm dünner Halm ist von der Seite sonst unsichtbar),
  Kolben als Maske. Farben/Beleuchtung aus `grass.frag` übernommen.
- `grass.vert` zeichnet **kein Schilf mehr** (`uReedPass`), Schattenwurf läuft mit.
- Knöpfe: `DEMO_REEDPASS=0|1` (Standard 1), `DEMO_REEDRANGE`, `DEMO_REEDGRID`,
  `DEMO_REEDPER`, `DEMO_REEDDIST`. Selbsttest prüft den Build (Punkt 6).

**Eigenen Fehler gefunden:** der eigene `sinf`-Hash in `reed_cpu.c` warf bei
Weltkoordinaten Precision weg (Argument ~1e5) und erzeugte Kurzzyklen — die
Pflanzen einer Zelle stapelten sich, die Bilddeckung sank mit **mehr** Kandidaten
(26 % → 7 % → 21 %). Mit `c_hash12` (CPU-Zwilling von `hash12(vec2)`) wieder
monoton: 25,4 % / 35,5 % / 40,1 %.

**Messbank:** bester Schilf-Punkt der Welt (CPU-Scan): (204, 314), `DEMO_CAM="204 314 -0.10 0.5"`,
Arten-Farbmodus `DEMO_ARTEN=2` (Schilf = orange, in beiden Wegen dieselbe Farbe).

| `DEMO_REEDPER` | Pflanzen/m² in der Uferzone | Schilf-Bildpixel | Frame (VM) |
|---|---|---|---|
| Gras-Weg (alt) | ~198 Halme/m² gesamt | 107 239 px (46,5 %) | 1473 ms |
| 20 | 3 | 58 861 px (25,6 %) | 1483 ms |
| 40 | 6 | 71 921 px (31,2 %) | 1519 ms |
| **70 (Default)** | **10** | **86 630 px (37,6 %)** | **1585 ms (+7,6 %)** |
| 110 | 16 | 100 969 px (43,8 %) | 1794 ms (+22 %) |

Build: 13 689 Zellen, 12 856 Pflanzen, **15 ms** (neu alle 12 m Kameraweg).
Schatten-Wurf: 480 725 Texel (neu) gegen 479 502 (alt) — vergleichbar.

**Warum die Bilddeckung unter der alten liegt:** der alte Weg hat ferne Halme mit
dem Gras-`sizeScale` bis 2,8× hochskaliert; der neue hält echten Maßstab
(Plan-Punkt 4). Die ferne Riede wirkt dadurch dünner — **Optik prüft der Nutzer.**

## Session 2026-10-09, Runde 32 (Schatten: Kartenrand ausgeblendet, Auflösung gemessen)

**Gemacht:** `shadowAt` (alle sechs Kopien) mischt am Rand der Licht-Frustum weich
auf „voll beleuchtet" (7 % der Karte = 6,3 m). Ohne das wandert eine harte Linie
durchs Bild, sobald die Karte mit der Kamera läuft.

**Aufloesung der Schattenkarte gemessen, aber NICHT geaendert** (ein Baum-Wald,
Schatten an/aus, Differenz-Histogramm):

| Konfiguration | Texel | Schattenpixel | max. Abdunklung | Frame (VM) |
|---|---|---|---|---|
| 2048 / 45 m (ist so) | 4,4 cm | 12 484 | 69 | 2007 ms |
| 3072 / 45 m | 2,9 cm | 16 042 (+28 %) | 70 | 2033 ms (+1,3 %) |
| 2048 / 28 m | 2,7 cm | 27 372 | 43 (schwächer) | 2048 ms |

3072 wäre ein echter Gewinn (mehr Wurf bei 1,3 % Kosten), aber ob die Kante
dadurch *besser* wird, sieht nur ein Auge — die Messgroesse (Anzahl Schattenpixel)
sagt das nicht. Deshalb nichts geaendert; auf dem GPU-Host ist 3072 leicht drin,
wenn es dort scharf genug wirkt (eine Zeile in `main.c`: `SHADOW_SZ`).

## Zu prüfen, nachdem die Bäume umgebaut sind (Runde 27–30)

Knöpfe dafür (alle Umgebungsvariablen, kein Neubauen nötig):

**Tasten im laufenden Programm** (kein Neustart nötig zum Vergleichen):
`1` = Ganzbaum-Billboard der gröbsten Stufe aus/an (stößt einen Baum-Neubau an),
`2` = eigener Schilf-Pass aus/an. Verifiziert mit `xdotool key 1`: 3,59 % des
Bildes ändern sich, im Log steht der Zustand.

| Knopf | Wirkung |
|---|---|
| `DEMO_TREEFADE=<s>` | Dauer der LOD-Überblendung (Default 0,9 s; 0 = hartes Umschalten) |
| `DEMO_NOBB0=1` | gröbste Stufe wieder mit Einzelkarten statt Ganzbaum-Billboard |
| `DEMO_BB0CARDS=<div>` | Zusatzkarten **unter** dem Billboard der gröbsten Stufe (0 = nur Billboard; 8 = etwas Parallax-Struktur, kostet +92 000 Karten und +3 % Frame) |
| `DEMO_CARDT0="<div> <scale>"` | Kartenanzahl/-größe der gröbsten Stufe, wenn Billboards aus sind |
| `DEMO_T2="<stride> <Zähler> <Nenner>"` | Vernetzung der mittleren Detailstufe (Default `3 3 4`) |
| `DEMO_TREEBUILDRANGE=<Zellen>` | Radius, in dem Bäume gebaut werden |
| `DEMO_TIERSWITCH=<s>` / `DEMO_TREEDEBUG=1` | Protokoll der Stufe-Wechsel und Satz-Wechsel |
| `tools/selftest.sh` | harte Prüfungen (0 übersprungene Bäume, keine Cap-Treffer, Bild deterministisch) |

## Session 2026-10-08, Runde 30 (Selbsttest-Bank, Herabstufung statt Weglassen, Sweep-Bänke)

**`tools/selftest.sh` (neu):** fährt feste Konfigurationen (Standardszene, Wiederholung,
jede Detailstufe eines Baumes, Kameraweg, kleinster/grösster Build-Radius, ohne Gras)
und prüft hart: kein GL-Fehler, **0 übersprungene Bäume**, keine Cap-Treffer,
Bild über zwei Läufe **bitgleich**, Build/Upload läuft beim Kameraweg.
Aufruf: `tools/selftest.sh [BREITE HÖHE]`, Log unter `/tmp/64kdemo_selftest`.

**Gefunden und behoben:** bei `DEMO_LODRSCALE=2` fehlten **296 Bäume komplett**
(Gerüst 3 793 416/3 800 000, Blätter 1 595 260/1 600 000 voll). Neu wird bei
Budget-Überlast **herabgestuft statt weggelassen**: zuerst auf die gröbste Stufe
(Stamm 45 Vertices + 1 Ganzbaum-Billboard), dann die Einzelblätter weg, erst zuletzt
der ganze Baum. Stress-Konfiguration jetzt: 0 übersprungen, 297 herabgestuft.
Nebenbei: die Grössenschätzung pro Baum war für tier 0 viel zu hoch (3200 Vertices /
700 Karten statt 45 / 1) — das hat Budget verschenkt.

**Sweep-Bank `DEMO_T2="<stride> <Zähler> <Nenner>"`** (Vernetzung der mittleren
Stufe) — gemessen, **Default bewusst nicht geändert**:

| Einstellung | Gerüst Szene | Frame | Gerüst-Silhouette 25 m / 35 m / 50 m |
|---|---|---|---|
| Default (3, 3/4) | 2 834 673 | 1336 ms | Referenz |
| `4 2 3` | 2 515 179 (−11 %) | 1255 ms (−6 %) | −13,5 % / −16,7 % / −40,4 % Gerüstpixel |
| `5 2 3` | 2 379 459 (−16 %) | 1251 ms | — |

Grund gegen die Änderung: tier 2 gilt auch für kleine Bäume ab 25 m, dort fallen bei
`4 2 3` 13–17 % der Gerüstpixel weg (dünne Zweige verschwinden).

**Schatten gemessen (TODO 25, ein Baum in 16 m):** Grundfläche des Wurfs in der
Schattenkarte (2048², 4,4 cm/Texel): volle Krone 5838 Texel ≈ 11 m²,
tier 0 mit Billboard 9301 Texel ≈ 18 m². Auf dem Boden: Schatten an/aus ändert
1,24 % der Bildpixel (Mittlere Abdunklung 19,2); Blätter abschalten lässt den
Schatten fast gleich bleiben (1,16 %) — die Kronen-Karten werfen ihn bereits, die
Einzelblätter tragen nur ~7 % bei. **Der alte Befund „nur Stumpf-Schatten" ist
damit messteilweise erledigt; die Optik muss der Nutzer prüfen.**

## Session 2026-10-08, Runde 29 (Ganzbaum-Billboard in der gröbsten Stufe, TODO 23)

**Umsetzung:** pro tier-0-Baum EIN Kamera-Quad statt hunderter Karten. Größe und
Mitte werden aus den echten Laubpositionen dieses Baumes abgeleitet
(`emit_crown_billboard`), die Beklebung kommt aus einer neuen prozedural erzeugten
Textur (2×2 Kacheln à 128 px): Laubsilhouette etwas breiter als hoch, Unterseite
flacher, Rand ausgefranst (Buschel ragen über die Silhouette hinaus), innen
Buschel-Sprenkel statt flacher Scheibe, farbneutral (Blattfarbe als Tint).
Abschaltbar mit `DEMO_NOBB0=1`.

**Gemessen (648 Bäume):** Karten gesamt 545 871 → **299 700** (tier 0: 246 535 →
**364**, eine pro Baum), Puffer 206 → 195 MB, Frame-Median 1448 → **1331 ms**.

**Kronendeckung, ein Baum, Maske, Terrain aus:**

| Abstand | tier 2 (Karten) | tier 0 (Billboard) |
|---|---|---|
| 100 m | 209 px | 517 px |
| 134 m (Schaltweite 20× Wuchshöhe) | 66 px | 267 px |
| 160 m | 31 px | 175 px |

Der Sprung geht bewusst Richtung „voll“: eine 30 px hohe Krone in 134 m ist ein
~16×16-px-Fleck (267 px), die Kartenstufe liefert dort nur 66 px — das war der
Nutzerbefund „ferne Bäume verlieren ihre Blätter“. Ganze Szene: grünes Laub
2160 → 7606 Pixel, 2,63 % der Pixel verändert.

**Offen:** Der Dichtesprung an der Schaltweite 2→0 bleibt trotz Überblendung
sichtbar (Karten dünnen aus, Billboard füllt). Gegenmaßnahme: Kartengröße oder
-anzahl im äußeren Bereich der Stufe 2 shader-seitig hochregeln.

## Session 2026-10-08, Runde 28 (Kronen-Karten als echte Instanzen)

**Ausloeser:** nach dem Doppel-Puffer (Runde 26) waren ~876 MB CPU und genauso viel
VBO noetig. Eine Karte kostete 6 Eckpunkt-Instanzen a 13 Float = **312 Byte**.

**Aenderung:** eine Instanz pro Karte (11 Float = 44 Byte): center(3), size, rot,
seed, variant, lodR, col(3). Die Ecke kommt aus einem statischen 6-Vertex-Quad
(`vboCrownQuad`, Divisor 0), gezeichnet mit
`glDrawArraysInstanced(GL_TRIANGLES, 0, 6, nKarten)`. Der Shader bleibt unveraendert
(nur die Attribut-Offsets: aData 20->12, iLodR 36->28, iCol 40->32).

**Gemessen (640x360, 648 Baeume):**

| | vorher | nachher |
|---|---|---|
| Puffer pro Satz | 352 MB | **206 MB** |
| Karten-Datensatz | 3 275 226 Eckpunkte | **545 871 Karten** |
| Erstbuild | 490 ms (350 + 140) | **373 ms** (290 + 83) |
| CPU-Doppel-Puffer total | ~876 MB | ~566 MB |
| Karten-Cap | 4 000 000 Eckpunkte | 700 000 Karten (gemessen 545 871, 0 uebersprungen) |

**Bildvergleich alt/neu** (feste Kamera, Terrain aus, Freeze): 336 von 230 400
Pixeln (0,15 %) unterscheiden sich, alle in einem 9x57-Pixel-Streifen am rechten
Bildrand. Mit `DEMO_SHADOW=0` sind beide Staende **bitgleich** (0 Pixel) — die
Differenz kommt aus dem Schatten-Wurf (eine ferne Krone am Rand der Schattenkarte),
nicht aus der Geometrie. Zwei Laeufe desselben Standes sind bitgleich.

## Session 2026-10-08, Runde 26 (Baum-Build im Worker-Thread, Upload in Scheiben)

**Nutzer:** „dann sollten wir uns als naechstes mal um das stocken kuessmern.“ / „ich nehme B“
(Thread wie beim Gras).

**Umgesetzt** (Muster von grass_cpu.c: request → Worker baut zu Ende → Main-Thread
holt ab → Upload im Main-Thread):

- `trees_build_begin / trees_build_step(maxTrees) / trees_build_finish` statt eines
  Aufrufs; `trees_build_request(tx,tz,lodRef)`, `trees_build_busy()`,
  `trees_build_finished_snapshot()`, `trees_thread_start()`.
- **Doppel-Puffer** CPU-seitig: der Worker schreibt in `treeVerts/crownVerts/leafInst`,
  gezeichnet wird aus `treeVertsD/crownVertsD/leafInstD`; getauscht wird in
  `trees_build_finish()` unter Mutex (Mutex/Cond wie beim Gras, „finish then latest“).
- **Zwei VBO/VAO-Saetze** (`vboTree[2]`, `vaoTree[2]`, …, `drawSet`): der Upload
  laeuft in den Hintergrund-Satz, `drawSet` wechselt erst nach dem letzten Slice —
  die GPU sieht nie halbhochgeladene Geometrie.
- **Upload in Scheiben** mit Zeitbudget: 25 % der letzten Framezeit, geklemmt auf
  3..20 ms (`UP_BYTES` 6 MB pro Slice). `glBufferSubData` neu geladen (LOADGL2).
- Nur **ein Build gleichzeitig** in Arbeit (Trigger prueft `!upActive &&
  !trees_build_busy()`), sonst laeuft die Kamera dem Build davon.

**Gemessen (VM, llvmpipe, 640x360, `DEMO_WALK="13 0"`):**

| | vorher | jetzt |
|---|---|---|
| Blockierung im Frame beim Stufenwechsel | 490 ms (350 Generieren + 140 Upload) | 0 ms — Generieren im Thread |
| Kosten pro Frame waehrend des Uploads | — | 3..20 ms (Zeitbudget) |
| Frame-Median waehrend Walk | ~1700 ms | ~1384 ms |

**Richtigkeit:** nach 32 m Kameraweg ist das Bild an der Zielkamera identisch zum
Stillstand-Test an derselben Kamera (je 2386 Baum-Gruen-Pixel, Y-Bereich 0..333,
Terrain aus). 648 Baeume, 0 Cap-Treffer, 0 uebersprungene Baeume.

**Zwischenfall beim Einbau:** zuerst festes Kontingent 24 MB/Frame — das neue Set
war in der VM erst nach ~15 Frames (20 s) dran, die naechsten Baeume fehlten
sichtbar (Bildvergleich: nur 262 statt 2386 Gruen-Pixel). Deshalb Zeitbudget +
„nur ein Build in Arbeit“.

**Preis:** Doppel-Puffer. Caps gesenkt auf gemessen + Reserve
(`TREE_VERT_CAP 5,2 M -> 3,8 M`, `CROWN_CAP 4,2 M -> 4,0 M`), pro Satz ~438 MB,
in Summe ~876 MB CPU und dieselbe Groesse in VBOs. Das muss runter — TODO 23
(Ganzbaum-Billboard) und vor allem **Karten als echte Instanzen** (heute 6
Eckpunkt-Instanzen a 13 Float = 312 Byte pro Karte, eine instanzierte Karte waeren
52 Byte) senken die Datenmenge um ~6x und damit auch Upload und Build-Zeit.

## Session 2026-10-08, Runde 25f (Stocken: Messbank eingebaut, Fernstufe entlastet)

**Nutzer:** „dann sollten wir uns als naechstes mal um das stocken kuessmern.“

**Messbank:** Frame-Zeit-Statistik (median/p95/max, alle 2 s) und `DEMO_WALK="vx vz"`
(fester Kameraweg in m/s) in main.c — ohne Tastatur reproduzierbar.

**Befund 1 — Rebuild blockiert den Frame:** Ausloeser ist `|camTgt − Buildzentrum| > 12 m`
(main.c, seit Runde 25 mit echter Kamera als LOD-Referenz). Kosten gemessen:
`Generieren 350 ms + Upload 140 ms = 490 ms` (352 MB Puffer) — synchron, also ein
sichtbarer Einbruch alle ~12 m Kameraweg.

**Befund 2 — Frame-Zeit skaliert mit der Geometriemenge** (VM, llvmpipe, 640x360,
`DEMO_LODRSCALE` skaliert die Schaltweiten, also den Detailanteil):

| LODRSCALE | Geruest | Karten-Ecken | Blaetter | Frame median |
|---|---|---|---|---|
| 0,3 | 2,36 M | 2,06 M | 0,06 M | 1179 ms |
| 1,0 | 4,08 M | 2,91 M | 1,05 M | 1735 ms |
| 2,0 | 4,44 M | 2,36 M | 1,60 M | 2605 ms |

**Aenderung (1): tier 0 ohne Zweigroehren.** 224 ferne Baeume kosteten 0,78 Mio der
4,08 Mio Geruest-Vertices; ihre Aeste sind 1..2 px gross und liegen in den Laubkarten.
`meshBranch = !(tier == 0 && lvl > 0)` — Stamm bleibt, Zweigroehre entfaellt.
Gleichzeitig `wantCards` tier 0 von `ezNSprig/4` auf `/3`: die ferne Krone deckte
34 % weniger Pixel ab als die nahe (1060 vs 1618 Kronenpixel bei 40 m). Gemessen
nachher: **1298 vs 1618 (−20 %)**, Farbe identisch (69/127/46 vs 70/128/47).
Ergebnis: Geruest 4,08 M -> **2,83 M (−31 %)**, Puffer 393 -> 352 MB,
0 uebersprungene Baeume (CROWN_CAP 3,6 M -> 4,2 M, weil Karten 2,91 -> 3,28 M).
Frame-Zeit in der VM kaum geaendert (1735 -> ~1690 ms): die eingesparten Vertices
wurden durch mehr Karten (Full-Rate) aufgebraucht.

**Offen (2): den Rebuild aufteilen.** Vorgesehen nach Gras-Muster
(`grass_build_begin/step/finish` + Worker-Thread): Generieren in Baeumen pro Frame
in einen zweiten Puffersatz, Upload in Scheiben (~24 MB/Frame), Swap wenn fertig.
Ziel: kein Frame ueber ~30 ms durch Rebuild. Dazu kommen TODO 23
(Ganzbaum-Billboard) und die permanente Baum-Geometrie pro Baum (Slot-Verwaltung),
damit nicht der ganze Wald neu gebaut werden muss.

## Session 2026-10-08, Runde 25e (Blattfarbe: Kette gemessen in Ordnung, Palette neu gewaehlt)

**Nutzer:** „hast du die blattfarbe noch auf dem schirm?“ / „ja das angleichen der
farbe hatten wir schon. aber die farbwahl gefaellt mir noch nicht so…“

**Messung der Stufen-Konsistenz** (ein Baum, 40 m, `DEMO_TIER` erzwungen, Maske aus
Maskenlauf + Farbe aus Normallauf, Terrain aus): tier 3 (1618 Kronenpx,
72/110/29), tier 2 (1564 px, 72/110/29), tier 0 (1060 px, 73/110/30) → Farbe Δ ≤ 1
pro Kanal. Die Kette (eine Quelle `ezLeafR/G/B`, farbneutraler Atlas, gleiche
Beleuchtung in `leaf.frag`/`crown.frag`) haelt.

**Messfehler vorheriger Runden:** die gruenen Bildmasken der Gesamtansicht
enthielten Gras — deshalb sahen „Karten“ blaecher/heller aus (130/165/108 vs
141/174/122). Ohne Terrain (oder mit Differenzmaske an identischer Kamera) ist der
Unterschied real nicht vorhanden.

**Was in der Ferne wirklich anders ist: die Deckung, nicht die Farbe.** tier 0 malt
1060 statt 1618 Kronenpixel (−34 %). Grund rechnerisch: `wantCards = ezNSprig/4`
bei `CARDSCALE 1,30` → Deckung 0,25·1,69 = 0,42 gegen 0,5·1,5625 = 0,78 in tier 3.
Noch offen: `wantCards` in tier 0 auf `ezNSprig/2` (kostet ~19 000 Karten) oder
`ezNSprig/3` mit `CARDSCALE 1,45`.

**Palette:** neu als HSV-Bereiche `ezHueMin/Max`, `ezSatMin/Max`, `ezValMin/Max`
(trees_cpu.c), zur Laufzeit justierbar: `DEMO_LEAFPAL=aktuell|saftig|frisch|tief|wald`
oder `DEMO_LEAFHSV="hmin hmax smin smax vmin vmax"`. Gerendert gemessen (40 m,
Einzelblaetter, Terrain aus):

| Palette | HSV hue/sat/val | gerendert | Saett. | G/R |
|---|---|---|---|---|
| aktuell (alt) | 72–144° / 0,62–0,95 / 0,24–0,46 | (72,110,29) | 0,73 | 1,53 |
| frisch | 86–115° / 0,55–0,75 / 0,45–0,65 | (103,136,68) | 0,50 | 1,32 |
| saftig | 93–133° / 0,60–0,85 / 0,34–0,55 | (75,123,50) | 0,60 | 1,64 |
| **wald (neu, Standard)** | 101–130° / 0,65–0,88 / 0,38–0,58 | **(70,128,47)** | 0,63 | 1,82 |
| saftig+ | 101–137° / 0,65–0,90 / 0,38–0,58 | (63,128,44) | 0,65 | 2,02 |
| tief | 108–151° / 0,70–0,92 / 0,26–0,40 | (39,111,43) | 0,65 | 2,85 |
| blau-gruen | 130–162° / 0,55–0,75 / 0,35–0,55 | (69,124,84) | 0,45 | 1,80 |

Der alte Wert 72° (Gelb-Gruen/Oliv) war der Hauptgrund fuer „herbstlich“; die
Untergrenze auf 101° zu schieben nimmt ihn heraus. Standard auf **wald** gesetzt.

## Session 2026-10-08, Runde 25d (EZ-Tree headless ausgeführt — Terminalast gefehlt)

**Nutzer:** „ich sehe 1 und zwar bei dem Stamm. der hört irgendwann weiter zu
wachsen auf … Das original EZ Tree würde hier noch nicht abbrechen, oder? bei
5 5 16 ist die kamera direkt in einem baum der abgeschnitten ist.“

**Methode:** `npm install three` + `esbuild` → deren Generator läuft headless in
node (`/tmp/ezcmp.mjs`, `/tmp/ezlvl.mjs`). Damit sind die Referenzwerte gemessen,
nicht geraten.

**Referenz oak_medium (5 Seeds):** 183 Äste, 2660 Laubkarten, Stamm 37,24 von
65,6–67,7 Einheiten Baumhöhe (Stammanteil 55–57 %), Krone 41–46 Einheiten breit,
Laub beginnt bei 22–29 % der Baumhöhe.

**Fund:** deren `#growBranch` schiebt für **jeden** Nicht-Terminalast einen
*Fortsetzungszweig* an die Spitze (`tree.js:424-438`): gleiche Richtung, Radius =
Spitzenradius, Länge `length[level+1]`, **Section-/Segmentzahl wie der Elternast**.
Deshalb hat die Referenz 4 „Stämme“ mit 9 Ringen: Stamm 0→37,2, dann 37,2→48,1,
48,1→59,0, 59,0→65,6. Unser Port hatte das nicht → der Stamm endete bei 55 %
Höhe mit Kappe, die Krone wuchs nur 18 statt 30 Einheiten über die Stamm spitze.
Nebenbefund: deren `sectionLength`-Division durch `(levels-1)` ist toter Code
(String-Vergleich `'Deciduous'` gegen den Typwert `'deciduous'`) — unser Port
skaliert bereits korrekt ohne diese Division (gemessen: Abschnitt 4,655 = 37,24/8).

**Fix:** `ez_grow()` bekommt `secOv/segOv` und schiebt den Terminalast selbst.

**Messung danach (Baum bei 5,2/16, preset oak, unit 0,152):**

| Größe | Referenz | wir vorher | wir jetzt |
|---|---|---|---|
| Baumhöhe | 9,97–10,29 m | 8,39 m | **10,19 m** |
| Kronenbreite | 6,23–6,99 m | 6,43 m | **6,43 m** |
| Laub beginnt bei | 22–29 % Höhe | 23 % | **18 %** |
| Gerüst Vertices | — | 4 335 | 9 993 |

**Kosten im Wald (Kamera 5/16, 960×540):** 648 Bäume, 4,08 Mio Gerüst-Vertices,
484 246 Kronen-Karten (= 2,91 Mio Eckpunkt-Instanzen à 13 Float), 1,05 Mio
Einzelblätter, 393 MB Puffer, Rebuild 507 ms.
Caps erhöht: `TREE_VERT_CAP 3,6 M → 5,2 M`, `CROWN_CAP 2,6 M → 3,6 M`
→ **0 übersprungene Bäume** (vorher 140). Fern-LODs dominieren weiter
(tier 0: 224 Bäume / 0,78 Mio Vertices) — das Billboard (Plan S5) ist der
nächste Schritt, um diese Kosten zu halbieren.

**Makefile-Falle:** `$(BIN)` hing nur an `$(SRC)`, nicht an den Headern — eine
`scene.h`-Änderung wurde nicht gebaut (Cap blieb scheinbar wirkungslos).
`HDRS = $(wildcard src/*.h)` ergänzt.

## Session 2026-10-08, Runde 25c (Budget erhoeht — Nutzer: „keine Loesung, Budget erhoehen, auch pro Baum")

Pro Baum **gemessen** (DEMO_TREELOG, drei Kamerapositionen):

| Stufe | max Geruest-Vertices | max Karten | max Blatt-Instanzen |
|---|---|---|---|
| tier 3 | **30 096** | 1 230 | **12 300** |
| tier 2 | 8 016 | 1 230 | 12 300 |
| tier 0 | 2 988 | 615 | 0 |

Wald-Summen: Geruest 2,40-2,84 Mio, Karten 295-321k, Blaetter 0,55-0,77 Mio.
Davon abgeleitete Budgets (mit Reserve):
`TREE_VERT_CAP 2 700 000 -> 3 600 000`, `CROWN_CAP 1 800 000 -> 2 600 000`,
`LEAF_CAP 1 200 000 -> 1 600 000`, und pro Baum `EZ_SPRIGCAP 4 096 -> 16 384`
(groesster Baum 4 920 Laubansaetze — deshalb fielen bei ihm still Karten weg).
Die Budget-Schaetzung pro Baum ist auf die gemessenen Maxima angehoben
(31 000 / 8 500 / 3 200 Geruest, 12 500 Blaetter).
Gemessen nachher, drei Kamerapositionen: **0 Cap-Treffer, 0 uebersprungene Baeume**,
Rebuild 299-342 ms (Generieren 211-242 + Upload 88-100), Puffer 242-270 MB,
Prozess-RSS 984 MB.
Nebenbei entfernt: `ezTip*`/`EZ_TIPCAP` — geschrieben, aber nie gelesen (toter Code,
Cap 48 war zu klein und haette fast jeden Baum falscho rollen lassen).
Rollback-Netz bleibt als letzte Sicherung (soll nie ausloesen).

## Session 2026-10-08, Runde 25b (Blattfarbe saettiger, Rebuild-Kosten gemessen)

- **Blattfarbe**: HSV `sat 0,45..0,90 -> 0,62..0,95`, `val 0,30..0,60 -> 0,24..0,46`;
  Ambient in leaf.frag/crown.frag von `(0,35,0,45,0,40)` auf `(0,18,0,32,0,16)`
  (graues Ambient woesch aus — Nutzerbefund „blass, herbstlich").
  Gemessen 16 m: (94,130,48) Sattigung 0,63 -> **(77,114,30) Sattigung 0,74**, G/R 1,48;
  60 m (72,109,30) Sattigung 0,72 — Stufen halten zusammen.
- **Abgesaegte Staemme / zu wenig Laub = volle Puffer**: Karten-Puffer war
  **1 800 000/1 800 000 voll, 54 Abbrueche**, Geruest 2 663 286/2 700 000 (98,6 %).
  Ein Abbruch mitten im Ast hinterlaesst einen offenen Ring = flaeche Schnittflaeche.
  Jetzt: vor dem Bauen wird geschaetzt, ob der ganze Baum ins Budget passt, sonst
  wird er uebersprungen (Bau nah->fern). Gemessen: **0 Cap-Treffer**, 585 Baeume
  gebaut, **55 uebersprungen**. Cap-Waechter + Uebersprungs-Zaehler im Log.
  => TODO 23 (Ganzbaum-Billboard, ~6 Vertices pro Baum) ist die eigentliche
  Antwort: damit sind ferne Baeume immer da, ohne Budgetdruck.
- **Rebuild-Stottern gemessen**: `Baum-Rebuild: Generieren 214 ms + Upload 91 ms
  = 305 ms (242 MB Puffer)` — ausgeloesst alle 12 m Kameraweg (camTgt). Das ist
  die Antwort auf die Nutzerfrage: ja, wie beim Gras verteilen (Baeume pro Frame
  abzuarbeiten) und/oder eigener Thread mit Doppel-Puffer; der Upload von 242 MB
  ist der teurere Teil und laeuft im Main-Thread.

## Session 2026-10-08, Runde 25 (LOD-Farbe: drei Fehler gefunden und gemessen)

Nutzerbefund: Stufen besser, aber Karten weiterhin falsch gefärbt; ferner Baum ohne
Stamm; **in der fertigen Version starke Abweichungen zwischen den LODs**.

Messbank (neu):
- `DEMO_NOTERRAIN=1` — Terrain/Himmel/Gras aus, schwarzer Hintergrund
- `DEMO_LODRSCALE=<f>` — Schaltweiten skalieren, damit an EINEM Standort beide
  Repräsentationen vergleichbar sind
- `DEMO_TREEDIST` sucht jetzt die hindernisfreie Blickrichtung (vorher lag der
  Baum bei 80 m hinter einem Hügel -> Messung zählte 0 Gerüst-Pixel)
- `DEMO_FREEZE=2` ist Pflicht: ohne Zeitfreeze liegen Blätter/Karten pro Lauf
  leicht anders -> Pixelmengen und Farben nicht vergleichbar
- Masken-Diagnose (LEAFMASK/CROWNMASK) greift erst NACH dem Discard; vorher
  zählte sie auch verworfene Texel -> alle früheren Farbmessungen waren falsch

Gefunden und behoben (jeweils gemessen):
1. **Karten wurden nur für beim Bau ferne Bäume gebaut**
   (`wantCards = (d >= R1*0.9) ? ... : 0`). Die Stufe gilt aber nur bis zum
   nächsten Baum-Bau (alle 12 m Kameraweg) -> ein nah gebauter Baum verliert beim
   Entfernen alles Laub, weil nichts übergibt. Jetzt Karten in JEDER Stufe
   (tier-3-Bäume: 0 -> 51096 Karten, gesamt 275142 -> 300000).
2. **Atlas-Kachel war eine ganze Referenzkrone mit eigener Zufallsfarbe**; jetzt
   ein Blattbüschel (9..13 unserer Blätter, Deckung median 27 %) und FARBNORMAL
   (gemittelt 241/241/241). Die Blattfarbe DIESES Baumes steckt in der
   Karten-Instanz (CROWN_FLOATS 10 -> 13) und wird in crown.frag multipliziert.
3. **Hauptfund: `uAlbedoOnly` blieb nach der Atlas-Kapture auf 1 stehen** (Uniform
   bleibt pro Programm erhalten, nie zurückgesetzt) -> die GESAMTE Blatt-
   Rendrierung lief unbeleuchtet als rohe Albedo, während die Kronen-Karten
   beleuchtet wurden. Deshalb sprang die Helligkeit am Stufenwechsel.
   Gemessen am selben Baum, 16 m, Zeit eingefroren, identische 2268 Laub-Pixel:
   vorher Blätter (39,82,11) gegen Karten (74,104,42); nachher (94,130,48) gegen
   (74,104,42). Abstandsdurchgang nachher: 16 m (94,130,48), 40 m (87,124,46),
   60 m (87,124,47), 100 m (87,121,60) — Farbe hält jetzt über alle Stufen.

Offen: Ganzbaum-Billboard (TODO 23) — der fehlende Stamm in großer Entfernung ist
noch nicht blindenfrei gemessen (40 m: 142 Gerüst-Pixel, 80/160 m: 0, aber die
80/160-m-Läufe waren vermutlich Hügel-OKklusion).

## Session 2026-10-08, Runde 24 (EZ-Tree-Port, LOD-Semantik, Referenzbilder)

### Commits
- `3a3be11` Feinast-Gerüst (Generator A) + Messbank `DEMO_ONETREE`/`DEMO_TIER`/`DEMO_TREEDIST`
- `30d4752` **EZ-Tree-Port** (`ez_grow`), Hooks `DEMO_TREEGEN`, `DEMO_EZPRESET`, `DEMO_EZSPREAD`, Welt-BBox + Stammradien im Log
- `971873e` **LOD nach EZ-Tree-Semantik** + hängende Zweige behoben

### EZ-Tree-Port: was 1:1 übernommen ist
`src/trees_cpu.c`, Funktion `ez_grow()`. Preset-Tabelle `EZ_PRESETS` mit den
Werten aus `ez-tree/src/lib/presets/*_medium.json` (oak, ash, aspen, pine):
`levels`, `children`, `angle`, `length`, `radius`, `start`, `gnarl`, `twist`,
`taper`, `force`, `leafCount/leafAngle/leafStart/leafSize`.
- Abschnitts-Ast: `sec[0..S]`, Wachstum entlang der lokalen Achse, Rahmen via
  `ring_transport` (bestehender Code), `twist` pro Abschnitt.
- Gnarliness: `gnarl[lvl] * max(1, 1/sqrt(r))` — **Radius in Preset-Einheiten**
  (`r / unit`), sonst skaliert der Fehler mit unserem Größenfaktor.
- Wachstums-Kraft: Drehung um `cross(d, up)` mit `step = force / r`, begrenzt auf
  `atan2(|cross|, dot)`.
- Kinder: geschichtet `start + (i + hash) * (1-start)/cnt`, Radial-Slots
  `2π(radialOff + (slot + ±0,5)/cnt)` mit **vertauschter Slot-Zuweisung**.
- Kinder-Radius immer `lerp(Elternradius) * radius[childlvl]`.
- Radius: `base.r * (1 - taper[lvl] * i/S)`, Evergreen `1 - i/S`, letzter
  Abschnitt des letzten Levels → 0,001.
- Abweichungen von 1:1 (bewusst): `treeEzSpread` (Kronenlevel-Längen, Default 1),
  `kidPct` (Kinderzahl fern 60 %, mittel 85 %), Laubkarten auf unseren
  Atlas-Quads statt eigenen Blattreben-Texturen.

### Fehler dieser Runde (alle mit Messung gefunden)
1. `force/radius` und `1/sqrt(radius)` sind einheitenabhängig. Nach `unit`-Scaling
   wirkten sie ~10× zu stark → Äste wurden nach oben gezogen, Krone 26 px breit.
2. `taper` geraten (0,35) statt ihrer Werte (Eiche 0,73/0,42/0,69/0,75) → Stamm
   endete dick („abgesägt“). Zusätzlich eine **erfundene** da-Vinci-
   Flächenabrechnung, die den Stamm auf 2 cm einbrechen ließ — wieder entfernt.
   Nach dem Fix: Stammradien 0,172 → 0,047 m durchgehend.
3. LOD aus der Ursprungscodebasis (Struktur weggeworfen) → Inhaltswechsel fern.
4. Kinder/Laub auf gebogenem Pfad statt auf vernetzter Sehre → Zweige in der Luft.

### Messbank (neu)
```sh
DEMO_TREEGEN=alt|ez            # Generator wechseln (Standard ez)
DEMO_ONETREE=1                 # ein Baum, kein Gras, Kamera davor (10-26 fps)
DEMO_TIER=0|2|3                # Detailstufe erzwingen
DEMO_TREEDIST=<m>              # Ganzbaum-Abstand (NICHT vergessen, sonst Nahansicht)
DEMO_EZPRESET=0..3             # 0 oak, 1 ash, 2 aspen, 3 pine
DEMO_EZSPREAD=<f>              # Kronenlevel-Längen (1 = 1:1-Port)
```
Log pro Build: `Baeume um …`, `BBox: Breite/Hoehe/Tiefe`, `Preset … unit …
Stammradius … s= … hf=`, `Stammradien (m): …`.

### Referenz-Recherche (Antworte auf „woher die schöne Rinde?“)
- `quicktree` (Sapling-Frontend): **keine** Texturen im Repo, nur
  `bark_base_tex`/`bark_normal_tex`-Datei-Properties mit hartkodierten
  Autor-Pfaden (`bark1.tga`, `bark1_nmp.tga`).
- `EZ-Tree` (`dgreenheck`, Three.js, **MIT**): liefert 11 Rinden-Sets
  (`Bark00x_1K-JPG` = Color + NormalGL + Roughness, 512×1024) und Blatt-Texturen
  (oak/aspen/ash/pine, 1024² RGBA) — als **Git-LFS** (im Clone nur Zeiger;
  Download über `media.githubusercontent.com/media/<repo>/main/...`).
- Blattkarten sind durchgängig **Reben**, nicht Einzelblätter:
  Segmente/Zeile Median oak 4 (max 13), ash 6 (max 19), pine 13 (max 38),
  quicktree-Beispiel 3 (max 9). Konvention: Stiel unten Mitte.
- Ihr Look = Geometrie + Foto-Color/Normalmap/Roughness + `MeshStandardMaterial`
  + **Neutral Tone Mapping, Exposure 2**. Ihr Schattenwurf ist **schwächer** als
  unserer (512² über ±100 Einheiten, normalBias 0,2 vs. unsere 2048² über ±45 m).
  Bei uns fehlt Tone Mapping/Exposure völlig (nur `pow(col, 1/2.2)`).
- EZ-Tree-LOD: `defaultLODLevels` (distance 0 / 100 / 250, Hysterese 0,05,
  `sectionStride` 3/6, `segmentFactor` 0,75/0,4, `leafStride` 2,
  `leafScale` 1,25/1,3 mit Begründung „volle Kompensation liest sich als
  Ballon-Blätter“, äußerste Stufe `billboard: Single`).

## Session 2026-10-07, Runde 22 (Shadow-Map + Nutzer-Feedback)

**Shadow-Map eingebaut** (1024², eine orthogonale Licht-Kaskade, 3×3-PCF, Wirft
Baum-Mesh + Kronen-Quads + Einzelblätter). Drei echte Fehler dabei:

1. **`texture()` auf einer Depth-Textur ohne Mipmaps liefert auf llvmpipe Schwarz** —
   das ganze Bild war schwarz (nur HUD sichtbar). `textureLod(uShadow, uv, 0.0)`
   funktioniert. Umgestellt auf 16-bit-Depth + explizite Lod.
2. **Depth-Funktion nicht zurückgesetzt:** der Schatten-Wurf setzt `GL_LESS`, der
   Szenen-Pass braucht `GL_LEQUAL` (Himmel schreibt `gl_FragDepth = 1.0` gegen
   gecleartes 1.0). Folge: Himmel schwarz, Rest intakt. Jetzt `GL_LEQUAL` nach dem
   Wurf wiederhergestellt.
3. **NDC vs. Depth-Bereich:** `lp.z` aus der Licht-Matrix liegt in [-1,1], die
   Depth-Textur enthält [0,1]. Ohne `z = lp.z*0.5+0.5` vergleicht man zwei Skalen →
   der Test fiel immer auf „beleuchtet“. Behoben.

**Messung nach Fix:** Shadow-Map zu 23,25 % gefüllt (Tiefen 0,0005…1,0),
Bildänderung durch Schatten 6,19 % der Pixel, mittlere Differenz 8,8.
**Aber: im Bild ist kein Schatten erkennbar** (Nutzer bestätigt) → TODO 20.

**Diagnose-Werkzeuge neu:** `DEMO_SHADOW=0/1`, `DEMO_SHADOWLEAF`, `DEMO_SHADOWFORCE=1`
(alles beschattet, prüft die Verkabelung), `DEMO_SHADOWDUMP=1` (Depth-Map als PPM),
`DEMO_CAM="x z [pitch]"` (Kamera exakt setzen), `DEMO_GLDBG=1`.
Fallen beim Messen: `glReadPixels` aus einem MSAA-FBO ist verboten (0x502) und hat
zwei Runden lang eine falsche Ursache vorgetäuscht; `glGetError()` löscht den Fehler,
man darf ihn nicht an zwei Stellen gleichzeitig abfragen.

**Nutzer-Feedback Runde 22 (wörtlich festgehalten, Reihenfolge der Abarbeitung):**
- „Das Schilf sieht noch nicht gut aus: zu dünne Halme und teilweise etwas zu dicht
  gewachsen.“ → TODO 19
- „Deine Optimierung der Gras-Sichtweite mit Übergang auf Bodentextur sieht nicht gut
  aus. Das echte Gras sollte viel weiter gezeichnet werden, weil es trotz AA-Artefakten
  viel besser als die Textur aussieht, die letztlich sehr auffällig periodisch ist.“
  → TODO 17
- „Deine Bäume: da musst du noch mal richtig recherchieren. Du hast recht grobe Äste und
  direkt an den Ästen zig kleine Blätter. Das erzeugt überhaupt keine volumetrische
  Baumkrone, sieht eher wie Tannennadeln aus.“ → TODO 18
- „Billboards der Baumkrone dürfen nicht mehr genutzt werden, wenn die Kamera direkt
  davor ist. Das sieht immer unecht aus.“ → TODO 16
- „Bei den ganz weit entfernten Bäumen ist der transparente Bereich der Textur
  weiterhin schwarz statt durchsichtig.“ → TODO 15

## Session 2026-10-07, Runde 21c (TODO 14: Blumen auf den Wiesen)

**Umsetzung:** zwei neue Arten in `species()` — 5 Mohn, 6 Lupine — plus ein echter
Gänseblümchen-Kopf. Kopf = Verbreiterung des Halmbands + Silhouette per Alpha-Test in
`grass.frag` (Mohn: roter Napf mit dunkler Mitte, Lupine: ähriger Stand aus
Einzelblüten, Gänseblümchen: weißer Teller mit gelbem Grund). Blütenköpfe werden im
Vertex-Shader **kammerzugewandt** orientiert, sonst sind sie von der Seite eine Linie.

**Vier echte Fehler, die ich erst finden musste:**
1. **Kopf an der Spitze wirkt nicht:** die Halmgeometrie ist bei t=0.55 am breitesten
   und läuft bei t=1 auf Breite **null** zu. Eine Verbreiterung am t=1-Vertex ändert
   nichts. Kopf jetzt als Buckel `headW = amp·sin(π·t)` ab t=0.55, `taper = max(taper, headW)`.
2. **Der Silhouetten-Discard frisst den Halm:** begann der Kopf bei t=0.5, hatten die
   unteren Strip-Dreiecke `vHead > 0.5` und wurden weggeschmissen (45 % Mohn → nur
   354 Bildpunkte). Kopf darf erst oberhalb der breiten Vertices anfangen.
3. **`rgf()` hat keine feste Skala:** bei niedriger Frequenz (0.017–0.023) liegt das
   ganze Sichtfeld in EINER Noiszelle — gemessen rgf-Maximum 0.33 bei Schwellwert 0.63
   → nie Blumen. Und ein zweites Feld mit eigenem Offset hatte eine andere lokale
   Mittelzahl (0.14 vs. 0.54) → Lupine erschien nirgends. Jetzt **ein** Feld (Frequenz
   0.05, Zelle ~20 m) mit **zwei Bändern** (Mohn 0.55–0.66, Lupine 0.455–0.515).
4. **Höhengate zu hoch:** `smoothstep(0.15,0.45,dh)` — das Nahfeld liegt bei dh 0.20–0.30,
   dort war das Gate halb zu. Jetzt `smoothstep(0.05,0.20,dh)` (nur über Wasserlinie).

**Messung** (Arten-Farbmodus, Kamera mitten in einer Blumenfläche, `DEMO_CAM="38 44"`):
Mohn **13,8 %**, Lupine **14,2 %**, Wiese 72 % der gefärbten Pflanzen-Pixel.
Ohne die Fixes: 0–17 Pixel. fps 2,03 → 1,86.

**Nebenprodukt — neue Test-Harnesses und ihre Fallen:** `/tmp/spectest.c` und
`/tmp/hashtest.c` werten die echte GLSL-Auswahl auf einem Raster aus und lesen sie zurück.
Drei Harness-Fehler, die jeweils eine falsche Diagnose geliefert hätten: Core-Profil
braucht ein gebundenes VAO (sonst wird nichts gezeichnet), drei Shader-Ecken auf einem
Punkt rasterisieren nichts (Vollbild-Dreieck über `gl_VertexID`), und RGBA8-Ausgabe
clamped Werte >1 (Art-Nummern müssen durch 6 geteilt werden).

## Session 2026-10-07, Runde 21b (TODO 13: Schilf)

Nutzer: „zu dicht und viel zu kerzengerade, Farbe sollte eher wie die Baumblätter
sein, vereinzelt dicke Halme mit dem ellipsoiden Bommel“.

- **Lichter:** `wReed *= 0.45` in `species()`. Gemessen (Arten-Farbmodus, untere
  Bildhälfte): Schilf-Anteil **59,9 % → 7,7 %** der gefärbten Pflanzen-Pixel.
- **Nicht kerzengerade:** `lean *= 0.25` → `lean *= 1.30`, und die Krümmung wandert
  nach oben (`lo = lean * pow(t, bendPow) * h * bendK` mit bendPow 1.55, bendK 1.30)
  — der Oberkopf neigt sich, wie bei echtem Schilf.
- **Farbe:** statt strohigem Gelb (0.44,0.46,0.24) jetzt grünes Laub wie die
  Baumblätter (0.16,0.33,0.13), Kolben braun-grünlich. Auch der Terrain-Unterton
  des Schilfgürtels wurde von gelb auf grün gestellt.
- **Dicke Halme mit Bommel:** pro Halm gehasht (`hash12 > 0.90`, ~10 %): 4,2 m hoch,
  2,2× breiter, und der Kolben ist ein Ellipsoid (`headW = 0.40 + (0.75+0.85·thick)·
  sin(π·ht)` statt einer festen Verbreiterung).
- Neuer Env-Hook `DEMO_ARTEN=2` (Arten-Farbmodus ohne Tasten — X-Tasten sind in der
  VM unzuverlässig). **Messfehler:** mit `DEMO_FREEZE` stehen die Gras-Fades still,
  der Screenshot landete mitten im „alle Halme sterben“-Zyklus → fast keine
  Pflanzen-Pixel. Ohne Freeze gemessen.

## Session 2026-10-07, Runde 21 (TODO 11: Rinde auf Ästen)

**Ursache der „texturlosen“ Äste:** `atan(n.z, n.x)` ist nur für **senkrechte**
Flächen die Umfangskoordinate. An einem waagerechten Ast zeigt die Normale nach
oben → der Azimut wird zufällig, das Muster kollabiert zu einer Fläche.

**Fix:** der Baum-Vertex trägt jetzt echte Rohr-Oberflächenkoordinaten
(`TREE_FLOATS` 10 → 12: `u` = 0..1 einmal um den Ring, `v` = Bogenlänge in m),
aus `emit_tube`/`emit_cap`. `barkH()` tastet Noise auf einem **Zylinder im Noiseraum**
ab (`n3(cos u · r, sin u · r, v·f)`) → nahtlos um den Umfang, auf jedem Rohr korrekt.
`circF` = Noiszellen **pro Umlauf**, deshalb skaliert die Rindenbreite automatisch mit
dem Rohrdurchmesser (dünne Äste = feinere Rinde, wie in der Natur).

**Messfehler auf dem Weg:** Attribut 3 war im VAO nie aktiviert
(`glVertexAttribPointer` ohne `glEnableVertexAttribArray`) → `vUV` war konstant 0,
die neue Rinde sah flach aus und verlor gegen die alte (Detail 1,09 vs 2,27). Nach dem
Aktivieren: Detail **2,27 → 4,00**, texturlose Baum-Pixel **57 % → 38 %** (gemessen auf
der exakten Baum-Maske ohne Blätter, Wind eingefroren).

Kosten: fps 2,01 (Standard) / 1,96 (Baum-Nahsicht) — kein messbarer Unterschied.

## Session 2026-10-07, Runde 20 (Nutzer-Feedback: Kronen-Redesign, Baum-Individuen)

**Nutzerbefund:** Blätter sind Quadrate; ferne Kronen zeigen Polygonkanten; die
hellgrünen Blattpolygone sind zu dünn („vereinzelt begrünte Zweige“); der
LOD-Übergang ist „eine Katastrophe“; Rinde endet an den Abzweigungen; jeder Baum
soll ein Individuum sein; ~100.000 Blätter pro Baum.

**Umgesetzt (Sapling-Vorbild, siehe Recherche oben):**
- **Blattsilhouette im Shader** (`leaf.frag`): eiförmiges Profil
  `hw = 0.5*aspect*sin(pi*t^0.55)^0.7` mit Sägeblattrand, Mittelrippe und Adern.
  Seitenverhältnis kommt pro Blatt aus `iData.w` (Sapling: `leafScaleX` 0,2–0,65,
  **niemals quadratisch**). Wirkt auch im Atlas, weil der Atlas mit demselben
  Shader aufgenommen wird.
- **Drei Hierarchieebenen**: Stamm → Aeste (5–9) → **Zweige** (6–16 pro Ast, eigene
  Rohre) — Zweige nur bei Bäumen < 11 m.
- **Blaetter sitzen AUF Aesten/Zweigen** statt in einem Zufallsellipsoid, mit
  **137,5°-Phyllotaxis** (goldener Winkel) und Neigung gegenueber dem Mutterast
  (zur Zweigspitze flacher), Blattgroesse zum Zweigende abnehmend (`leafScaleT`).
- **Jeder Baum ein Individuum**: Groesse hf 0,8–3,0, 5–9 Aeste, Kronenform aus
  5 Profilen (Astlaengen entlang des Stamms), eigene Blattfarbe (HSV), eigene
  Blattgroesse 5,5–11,5 cm, eigenes Seitenverhaeltnis, eigener Rindenton.
- **Blattbudget nah→fern sortiert**: Bäume wurden in Zell-Reihenfolge erzeugt, dadurch
  hatten ferne Bäume das LEAF_CAP schon leergeraeumt, bevor der Baum direkt vor der
  Kamera dran war. Jetzt Kandidaten einsammeln, nach Distanz sortieren, nah→fern bauen.
- **LOD-Pop entfernt**: die Zahl der Kronen-Quads wechselte mit der Detailstufe
  (3 → 18 bei 24 m) = sichtbarer Sprung. Jetzt **konstant 12 Quads pro Baum**, und
  die echten Blätter sterben im Vertex-Shader **statistisch** aus
  (`hash(iPos) > keep`, keep = 1−smoothstep(20,28 m)) — Dichte faellt sanft, statt
  schlagartig beim Rebuild.
- Kamera-Debugsprung `'` rahmt jetzt die **Krone** (braucht hf aus `tree_cell`).

**Messung:** 271.700 Einzelblaetter (1,6M Vertex), 7.668 Kronen-Quads, Blattgroesse
3,2–13,6 cm (Mittel 6,9 cm). Kronen-Nahschuss: grüner Blatt-Anteil der oberen
Bildhaelfte **8,07 % → 38,79 %**. fps 2,15 (vorher 55k Blätter) → 1,42 (300k, unsortiert)
→ **2,03** (272k, sortiert + Hash-Cull).

## Session 2026-10-07, Runde 19 (TODO 7: Blätter im Wind)

**Idee:** Gras, Äste, Einzelblätter und ferne Kronen müssen auf **dieselbe Böe**
reagieren — sonst steht der Wald still, während das Gras weht. Das Böen-Feld aus
`grass.vert` (`gp = xz*0.10 - (0.35,0.20)*uTime`,
`gust = noise(gp*2.3)*smoothstep(.55,.85,noise(gp))`) ist jetzt identisch in
`tree.vert`, `leaf.vert` und `crown.vert`.
- `tree.vert`: Astspitzen (sw=1) bis ~0,17 m Ausschlag bei Böe.
- `leaf.vert`: zwei getrennte Bewegungen — (a) Weltverschub wie der Ast (gleicher Ort,
gleiche Phase → Blatt und Ast schwingen zusammen), (b) lokales Flattern mit hoher
Frequenz, stärker bei Böen. Vorher wurde der Wind in der **lokalen** Blattachse
addiert, also je nach Blattrotation in eine zufällige Welt-Richtung.
- `crown.vert`: ferne Kronen bekommen denselben Ausschlag (bis 0,22 × Quad-Größe).

**Messung** (zwei Läufe mit eingefrorener Shader-Zeit t=3,5 s vs. t=11 s, Diff > 8):
`alter Wind: gesamt 6,20 % | Kronen 4,22 % | Gras 5,06 %`
`neuer Wind: gesamt 13,08 % | Kronen 19,03 % | Gras 9,94 %`

## Session 2026-10-07, Runde 18 (TODO 6b: ferne Kronen als Textur)

**Atlas statt gemalter Flecken:** `build_foliage_atlas()` nimmt beim Start **echte
Einzelblaetter** einer Referenz-Krone orthografisch auf — 4 Kronen-Varianten aus je 16
Richtungen = 64 Kacheln in einem 2048²-Atlas (256 px/Kachel, 2000 Blaetter je Krone,
Aufnahme dauert 0,09 s). Gespeichert wird Albedo + Abdeckung (leaf.frag hat dafuer
`uAlbedoOnly`), Beleuchtung passiert erst im Kronen-Shader (noetig fuer TODO 9).

`crown.vert` waehlt die Kachel nach **Blickrichtung**: `azIdx = floor(az*16/2pi)` aus
`atan(aCenter.xz - uCamPos.xz)`, Variante aus `aData.w` (pro Baum gesetzt). Die ferne
Krone parallaxiert also, wenn man sie umlaeuft, statt als flacher Aufkleber zu stehen.

**Zwei Fehler, die der Messung vorausgingen:**
1. `glClear` loescht den **ganzen** Framebuffer, nicht nur den Viewport — jede Kachel
   ueberschrieb alle vorherigen, am Ende war eine einzige Kachel im Atlas. Fix:
   `GL_SCISSOR_TEST` + `glScissor` auf das Kachelrechteck (vorher: 1/64 Kacheln gefuellt).
2. Kronen-Shader kompilierte nicht (`vTile` im Fragment-Shader nicht deklariert) und
   `make_program` gab eine **leere** Fehlermeldung aus — erst der Blick in die Log-Datei
   zeigte sie.

**Dichte-Iteration (gemessen an der Atlas-Deckung):** 572 Blaetter/0,05 m -> 2,8 %
Deckung; 884/0,11 m -> 14,8 %; 1320/0,15 m -> 18,6 %; 2000/0,19 m -> **22,9 %**
(= 50–61 % innerhalb der Kronen-Silhouette). Nahe Baeume: 805 -> **1550 Blaetter** und
6 innere Atlas-Quads dahinter; Kronen-Nahschuss dadurch Blatt-Anteil 7,7 % -> **38,5 %**,
Detail-Energie 1,30 -> 1,68.

**Kosten (sauber gemessen, ohne Streprocesses):** 55.800 Blatt-Instanzen kosten
2,22 -> 2,15 fps (−3 %). Atlas-Kronen sind sogar schneller als die prozeduralen
(1,33 vs 1,09 fps), weil nahe Baeume keine 18 Quads mehr zeichnen und frueher discarden.

**Wichtig fuer weitere Messungen:** es liefen zeitweise 8 alte Demo-Prozesse parallel —
das verfelscht jede fps-Zahl und erklart auch die als "Tastatur geht nicht" gedeuteten
Testlaeufe (Tasten gingen an ein altes Fenster). Raeumern: `pkill -9 -x 64kdemo`
(`-x` nach Prozessname, sonst bringt man sich selbst um).

## Session 2026-10-07, Runde 17 (TODO 6a: echtes Blattwerk an nahen Baeumen)

**Befund:** `leaf.vert`/`leaf.frag` waren seit Ewigkeiten fertig (Einzelblatt mit
Ueberhang, Transluzenz, Wind, LOD-Auscull), wurden aber **nie erzeugt oder gezeichnet** —
kein Blatt-Buffer, kein Draw-Call. Die Kopfzeile von `trees_cpu.c` behauptete seitdem
„240-360 individuelle Blaetter pro Baum“.

**Neu:**
- `leafInst` (LEAF_CAP 60.000 Instanzen a 10 floats: iPos, size/rotY/tilt, iCol) wird in
  `gen_tree()` gefuellt: Buschel an den 5 Astspitzen (45), an den Astmitten (20), an den
  10 Seittrieben (22) und 300 Punkte im Kronen-Ellipsoid — zusammen ~805 pro Baum.
- **Nur nahe Baeume** (< 50 m ums Build-Zentrum): dieselbe Grenze, ab der `leaf.vert`
  selbst auscullt. Ferne Baeume behalten die 18 Billboards, nahe nur noch 7 (Innenmasse),
  damit sich nichts doppelt.
- VAO/VBO + `glDrawArraysInstanced(GL_TRIANGLES, 0, 6, nLeafInst)` nach dem Kronen-Pass.
  Blatt ist Alpha-Test/ opak -> Depth schreiben, vor dem Gras.
- Messung im Kronen-Nahschuss: **10,5 % der Bildpunkte veraendert**, Detail-Energie
  1,16 → 1,39, satt-gruen 42,1 % → 45,2 %. Kosten: **keine** messbaren fps
  (Kronen-Nahschuss 2,19 fps mit und ohne, Default identisch).

**Fehler, der erst alles unverdaechtig machte:** der Multi-Edit, der VAO/VBO/Upload
anlegen sollte, war komplett fehlgeschlagen (ein Block matchte nicht -> gar nichts wurde
angewendet). Der Draw lief deshalb auf VAO 0 und brachte `GL_INVALID_OPERATION (0x502)`
pro Frame — und weil `save_screenshot_ppm()` bei jedem anhaengigen GL-Fehler abbricht,
war monatelang einfach **kein Screenshot mehr geschrieben** worden, obwohl die Konsole
„Auto-Screenshot“ druckte. Aufgespoert mit einer `uLeafMask`-Uniform (Blatt = reines Rot,
`DEMO_LEAFMASK=1`): zuerst 72 sichtbare Pixel, nach dem Fix 46.984.

**Zweiter Messfehler:** `tree_jump()` stellte die Kamera 2,4 m ueber die Stamm-Basis mit
Pitch −0,22 — die Krone sitzt aber bei `by + s*2,05`, also mehrere Meter hoeher, und war
nicht im Bild. Jetzt `dist = 3,6 + 0,9*s`, Hoehe +1,0 m, Pitch +0,55 (Krone im Bild).

## Session 2026-10-07, Runde 16 (TODO 5: prozedurale Rinde)

**Ursache der flachen Rinde (gemessen, nicht geraten):** `tree.frag` benutzte
`atan(vPos.x, vPos.z)` als Umfangs-Koordinate. Das ist der Winkel zum **Weltursprung**,
nicht zum Stamm. Ueber die Oberflaeche eines duennen Stamms in 30 m Entfernung aendert
sich dieser Welt-Winkel um ~0,01 rad — das Rauschen war damit praktisch konstant.
Nachgemessen mit exakter Baum-Pixelmaske: Farb-Streuung 2,7, Detail-Energie 0,05 —
also **keine** Struktur.

**Neu: Oberflaechen-Koordinaten aus der Normalen.** `u = atan(n.z, n.x)` (die Rinde
zeigt nach aussen, der Azimut der Normalen IST die Umfangsposition am Stamm) und
`v = vPos.y`. Dazu:
- `barkH(u,v)`: 3 Oktaven Rauschen mit Frequenzen u·7/17/37 und v·3/8/18 — Riefen sind
  1…3 cm breit und in die Laenge gestreckt (Umfangs-Frequenz hoeher als vertikale),
  plus **ridged noise** (`1-|2n-1|`) fuer tiefe Risse statt weicher Huegel.
- **Bump-Mapping**: endliche Differenzen von barkH entlang der beiden Flaechen-
  Tangenten (t1 = Umfang, t2 = "hoch"), storen die Normale -> Riefen werden bei
  seitlichem Licht sichtbar (auch ohne Geometrie).
- Verwitterungs-Flecken (fbm ueber u,v), zwei Rindentoenen (hell/dunkel) je nach
  Hoehe der Rinden-Oberflaeche.
- **Moos am Stammfuss**: `vCol.a` traegt den sway-Wert (0,05 Fuss … 0,30 Stammkopf …
  1,0 Astspitze) — damit gibt es eine relative Hoehe im Stamm **ohne** ein neues
  Puffer-Feld. Duenne Aeste (relH > 0,3) bekommen ausserdem weichere, kontrastaermere
  Rinde.

**Messung (Vorher/Nachher, identischer Frame):** neue Debug-Haaken, weil synthetische
X-Tasten in der VM nicht zuverlaessig ankommen:
- `DEMO_TREE=1` — Kamera beim Start direkt vor den naechsten Baum (`tree_jump()`,
  Taste `'` dasselbe)
- `DEMO_TREEMASK=1` — Baum-Mesh in reinem Rot = **exakte Maske** (Taste `;`)
- `DEMO_SHOT=<sek>` — automatischer Screenshot nach n Sekunden
- `DEMO_FREEZE=<sek>` — Shader-Zeit (Wind) einfrieren. Noetig, weil der Baum-Sway die
  Meshes zwischen zwei Laeufen um ~10 Pixel verschiebt; erst damit liegen Maske und
  Bild Pixel fuer Pixel uebereinander (Ueberschneidung 108.809 / 108.809).
Ergebnis ueber alle 52.250 Baum-Pixel mit vollem 7×7-Umfeld:
**Farb-Streuung 2,7 → 9,2**, **Detail-Energie |lum − Blur7×7| 0,05 → 2,11** (Faktor 42).

**Kosten:** Baum-Nahschuss (12 % Bildschirm) 2,74 → 2,58 fps; Standard-Ansicht
unveraendert 1,63 (Baeume sind dort nur 0,16 % der Pixel).

**Nebenbei:** die Zell-Platzierung in `trees_cpu.c` steckt jetzt in `tree_cell()` und
wird von `trees_build()` **und** dem neuen `tree_find_nearest()` gleich benutzt — der
Debug-Sprung landet also garantiert bei einem Baum, den es wirklich gibt.

## Session 2026-10-07, Runde 15 (TODO 4: offene Nähte in den Baum-Röhren)

**Ursache (gemessen, nicht geraten):** jedes Rohr-Segment baute sich eine EIGENE
orthogonale Basis aus seiner Achsenrichtung (up-Heuristik). An einem Gelenk
(Segmentende = Anfang des nächsten) liegen zwar dieselben Mittelpunkte, aber die
beiden Ringe sind um verschiedene Winkel gedreht → die Quad-Ränder teilen sich
nicht → **offene Schlitze**, durch die man durch Stamm und Äste sieht.
Messung mit `/tmp/seambuild.c` + `/tmp/seam.py` (Debug-Ausgabe der Ringe über
`-DTREE_SEAM_DEBUG`, Skript vergleicht Segmente mit gemeinsamem Gelenkpunkt):
**5751 Gelenke, Median der Lücke 11,1 mm, größte 40,7 mm, alle 5751 über 1 mm.**

**Fix: ein Ring pro Knoten, Parallel Transport.** Neuer `Ring` (Punkt, Radius, u, v)
pro Knoten; der nächste Ring wird mit Rodrigues-Drehung um `cross(d0,d1)` aus dem
vorherigen gewonnen, danach Drift-Korrektur (u senkrecht auf die neue Achse,
v = cross(Achse, u)). Damit sind die Ringe an einem Gelenk **identisch**:
**max Lücke 0,00 mm bei allen 5751 Gelenken.**

**T-Kreuzungen (Ast an Stamm, Seitentrieb an Ast):** der Anfangsring eines
Hauptastes liegt jetzt **auf der Stammachse** (Radius `min(0,075, 0,85·Stammradius)`),
damit steckt er ganz im Stamm und es gibt keinen Spalt; Anfangsringe von
Seitentrieben werden um das 0,9-fache des Elternradius in den Elternast
zurückgezogen.

**Kappen:** Stamm-Oberkante und die 5 Hauptastspitzen sind jetzt geschlossen
(`emit_cap`, Dreiecksfächer). Die 10 Seitentrieb-Spitzen bleiben offen — 10 Kappen
pro Baum (150 Vertices) hätten den Baum-Puffer über `TREE_VERT_CAP=700000` gehoben
(gemessen 743.796); die 0,9 cm offenen Enden stecken im Blattwerk.

**Zwei Folgefehler, die der neue Flächen-Test (`/tmp/tsurf.c`) gefangen hat:**
1. `kn[5]` (oberster Stammknoten) bekam nie `p`/`r` gesetzt → die Kappe lief von der
   Baumspitze in den **Weltursprung** (Dreiecke mit 1e25 m²).
2. Nach dessen Fix wurde `kn[5]` nie **transportiert** (Schleife lief zu kurz) →
   uninitialisierter Rahmen → Dreiecke mit 1e18 m.
Beide sind behoben; die Regel "Ring einer Kette muss immer Punkt, Radius UND u/v
bekommen" steht jetzt im Code, und die Ringe werden mit `memset` null-initialisiert.
Kontrolle: **Gesamtfläche aller Baum-Röhren 7.385 m²** (vorher 7.070 m² — plausibel,
etwas mehr durch dickere Astansätze und Kappen). Vor dem Fix waren es zeitweise
4,4e5 bzw. 1e25 m².

**Auswirkung im Bild:** braune (Baum-)Pixel 0,16 % → 0,16 % — die Bäume sehen aus wie
zuvor, nur ohne Schlitze. VM-fps 1,87 (unverändert), Baum-Vertices 575.100 → 647.946.

## Session 2026-10-07, Runde 14 (TODO 3: Pflanzen-Vielfalt)

**Fünf Pflanzentypen in weichen Regionen** (`grass.vert::species()`, `grass.frag`,
`terrainColor` in `fragment.glsl`, `c_reedW` in `grass_cpu.c`):
0 Wiese · 1 Hochgras · 2 Lavendel · 3 Gänseblümchen · 4 Schilf/Rohricht.
Verteilung im Initial-Build (30,34), gemessen mit `/tmp/test_species.c`:
**55,5 % Wiese, 16,6 % Hochgras, 3,9 % Lavendel, 3,8 % Gänseblümchen, 20,1 % Schilf.**

Botanische Zuordnung (weiche Rauschfelder, keine harten Regionengrenzen):
- **Schilf** nur an der Wasserlinie (dh = Höhe über Wasserlinie < 0,45 m) und dort in
  **Rieden** (Flecken von ~20 m, nicht das ganze Ufer entlang). Neu: Halme wachsen
  bis 0,12 m UNTER die Wasserlinie (vorher +0,10 m — die Uferzone fehlte ganz).
- **Lavendel** auf trockenen, höheren Hängen (dh 0,9…1,8 m).
- **Hochgras und Gänseblümchen** in der feuchteren Wiese (dh 0,15…0,60 m).
- Jede Art bekommt Höhe, Breite, Steifigkeit (lean), Kopfform (`taper`-Anhebung am
  oberen Ende = Blütenstand/Kolben) und eigene Farben (Grund/Spitze).

**Die Art entsteht ausschließlich im Shader.** Zuerst war sie CPU *und* Shader
(je 1:1-Port). Der Vergleichstest `/tmp/specmatch.c` (256×256 Positionen, Shader
gegen C-Port) ergab **3,3 % unterschiedliche Arten bei derselben Weltposition**:
`hash12` mit *stetigen* Argumenten ist nicht portabel — 1 ulp im Argument wird nach
`fract()` zu einem völlig anderen Wert. Die glatten fbm-Felder stimmen dagegen auf
**<1e-3** überein (der Test misst das pro Feld). Folge der Doppelrechnung wäre
gewesen: Halm-Höhe von der CPU-Art, Farbe von der Shader-Art. Deshalb setzt die CPU
Höhe/Breite/lean **nicht** mehr nach Art; CPU-seitig bleibt nur das Schilf-Feld
`c_reedW` als **Zell-Gate** (unter der Wasserlinie wächst nur Schilf).
Zwischenstand, verworfen: die CPU-Art in den Halm-Puffer zu schreiben, kostet 4 Byte
pro Halm (+25 % Speicher) — erübrigt sich mit der Shader-Lösung.

**Performance — drei Maßnahmen, sonst war es unbenutzbar:**
1. Arten-Rechnung mit 4-Oktaven-fbm brachte die VM von 2,0 auf **0,2 FPS**
   (5,8 Mio. Vertex-Aufrufe/Frame). Neu `rgf()` mit **2 Oktaven** (die Felder haben
   20…90 m Wellenlänge, feine Oktaven bringen dort nichts) und Schilf-Feld nur bei
   dh ≤ 0,45 (die allermeisten Halme sparen es komplett).
2. `grass.frag` rechnete die Regionen **pro Fragment** nochmal → jetzt als
   **Varying `vReg`** aus `grass.vert` (und `vBase` flog wieder raus).
3. Halm-Grundform **9 → 5 Vertices**: `GL_TRIANGLE_STRIP` über (v0,v1,v2,v3,v4)
   erzeugt exakt dieselben drei Dreiecke wie vorher `GL_TRIANGLES` mit duplizierten
   Vertices — **44 % weniger Vertex-Aufrufe**. (Culling ist aus, die alternierende
   Windung im Strip ist egal, die Normale kommt aus yaw.)
Endkosten der Aufgabe: **VM-fps 2,26 → 1,86** (`/tmp/bench.sh 60`), Halme 551k →
641k (Uferzone kam dazu), POS-Build 1 m 186 → 194 ms, Initial-Build 920 → 1420 ms.

**Debug: C Taste hat drei Modi** (durchschalten): AUS → FADE (gelb/cyan) →
**ARTEN** (grün Wiese, gelb Hochgras, violett Lavendel, weiß Gänseblümchen,
orange Schilf). Nachweis im Standbild (Startposition, unteres Bilddrittel):
grün 15,4 %, orange (Schilf) 7,6 %, gelb (Hochgras) 3,3 %, violett 0 % — korrekt,
denn die Startgegend liegt bei dh ≈ 0,4 m und Lavendel braucht dh > 0,9 m. Im
16-m-Einfangfeld (32,0/43,8) rechnet die CPU-Referenz (`/tmp/specdist.c`) 54,5 %
Wiese / 19,5 % Hochgras / 23,9 % Schilf / 0 % Lavendel — gleiche Größenordnung.
**Hinweis:** der P-Textur-Dump ist dafür unbrauchbar, der Einfang-Pass setzt
`uDbgCat=0` (nur Albedo ohne Debugfarben).

**Bildwirkung** (unteres Bilddrittel, Startposition, normaler Farbmodus):
"strohige" Pixel 5,4 % → **15,7 %**, grüne Bedeckung 4,5 % → **16,7 %**, keine
schwarzen Pixel.

**Noch offen an Aufgabe 3:**
- Lavendel liegt nur auf den Hügeln — vom Start aus meist außer Sicht.
- "Blüten" sind verbreiterte Halmspitzen, keine echten Blütenblätter/Stängel.
- Nur 5 Arten; gewünscht sind mehr (Klatschmohn, Kornblume, weitere Gräserarten).

## Session 2026-10-07, Runde 13 (TODO 2: Dunst + Sichtweite)

### Dunst: Einsatz erst ab 60 m, dann quadratisch
Vorher in 5 Shadern identisch: `1 - exp(-t^2 * 0.000015)` -> schon bei 110 m
**16 %** Dunst, gemischt in die helle Himmelsfarbe = milchig.
Neu: `fogAmt(t)` in `fragment.glsl` (FOG_ON=60, FOG_K=0.000012) -> 110 m **3 %**,
200 m 21 %, 300 m 50 %. Die vier Mesh-Shader (`tree/grass/crown/leaf.frag`)
benutzen dieselbe Formel inline (kein extra Function-Bytes pro Shader).
**Farbe bleibt `skyColor(rd)`**: eine eigene Dunstfarbe wuerde am Horizont eine
Naht erzeugen (ferner Boden muss exakt in den Himmel uebergehen).

Gemessen (1280x720, identische Fahrt, `/tmp/shot1.sh`, Auswertung `/tmp/haze.py`
und Zeilenband-Vergleich): Saettigung (max-min)/max pro Zeilenband ALT -> NEU
- oben/fern  y=0..60:   0.269 -> 0.297 (+10,5 %)
- fern       y=60..120: 0.326 -> 0.371 (+13,9 %)
- fern       y=120..180: 0.298 -> 0.350 (+17,3 %)
- Mitte      y=180..240: 0.235 -> 0.278 (+18,6 %)
- nah        y=660..720: 0.388 -> 0.406 (+4,5 %)
Der Blaustich geht in der Ferne am staerksten zurueck (mittlere Differenz
ueber das ganze Bild: R -5.2, G -2.1, B -5.7).

### Sichtweite
- `march()`: 128 -> **168** Schritte, Abbruch 250 -> **400 m**.
- `marchTerrain()` (Wasser-Reflexion): 96 -> 112 Schritte, 200 -> 300 m.
- `CAM_FAR` 300 -> **600** (scene.h) und `F0/N0` in `fragment.glsl` mitgezogen.
  Die Tiefenpraezision aendert sich kaum: PB = 2*zn*zf/(zn-zf) ~ 2*zn, also
  nahezubereichsbestimmt (bei zn=0.1: PB -0.2001 bei zf=300, -0.2000 bei 600).
- Baeume: `RANGE` 16 -> **20** Zellen (144 -> 180 m), `TREE_VERT_CAP` 420000 ->
  **700000**, `CROWN_CAP` 55000 -> **90000**, Crown-Fade 120..160 -> 150..185 m.
  Gemessen: 429 -> **640..655 Baeume**, 386100 -> **576000..589500 Vertices**,
  7722 -> **11520..11790** Canopy-Quads (Caps halten).

### Verifikation
- **Sichtweite:** vom Boden (2 m Kamerahohe) aendert sich fast nichts (flaches
  Land, 8.5 % -> 8.2 % Himmel oben). Auf ~12 m hochgeflogen (`/tmp/shot_up.sh`,
  Space halten) zeigt sich der Effekt deutlich: im Horizontband y=150..260 faellt
  der Himmel-Anteil von **49,7 % auf 15,6 %** — dort steht jetzt echtes Terrain,
  wo der March vorher bei 250 m aufgab.
- **FPS (VM, llvmpipe, 640x360, `/tmp/bench.sh`):** ALT 2,0 / NEU 2,0 — keine
  messbare Regression (VM ist raymarch-limitiert). Auf dem Host kosten die
  40 extra Schritte nur die Horizonte-Rays.
- Binary groesse unveraendert 80536 B (Caps sind nur BSS).

### Neue Werkzeuge
- `/tmp/bench.sh <sek>` — FPS-Mittel aus den `DBG fps`-Zeilen (Print ist jetzt in
  main.c alle 10 Frames, Debug-Zweig).
- `/tmp/shot1.sh <out.ppm>` / `/tmp/shot_up.sh <out.ppm>` — definierter
  Bildaufbau (6 s vor / 8 s hoch) fuer Vorher/Nachher-Vergleiche.
- `/tmp/xhold2.c` neu gebaut (Taste halten via XSendEvent + XFlush).
- `/tmp/haze.py` — Saettigungs-/Verwaschenheit-Messung ueber Zeilenbaender.

## Session 2026-10-07, Runde 12 (BUG schwarze Ferntextur GEFUNDEN + FIX)


### WURZEL: `grassTex` hatte NIE Mipmaps -> mipmap-incomplett -> Sample = schwarz
`glGenerateMipmap(target)` wirkt auf die **aktuell gebundene Textur**. In
`grass_capture()` war das die letzte von `ensure_fbo()` gebundene Textur
(**`texDepth`**), nicht `grassTex`. `grassTex` hat nur Level 0
(`glTexImage2D(..., 0, ...)`), aber `MIN_FILTER = GL_LINEAR_MIPMAP_LINEAR`
-> Textur ist **mipmap-incomplett**. Spec GL 4.6 §8.13.1: incomplett -> der
Sample liefert **(0,0,0,1)**, ohne GL-Fehler. Mesa/llvmpipe ist in diesem Pfad
nachsichtig (deshalb in der VM nie schwarz), NVIDIA nicht -> auf dem Host schwarz.

### Messkette (jede Stufe trennt was)
1. `grep uGrassTex src/shaders_gen.c` -> alle 6 Uniforms im generierten Shader.
2. Uniform-Orte: `tex=4 box=5 side=6 amt=7 far0=8 far1=9` — kein -1.
3. Textur direkt ausgeben (`return texture(...).rgb`): **nicht schwarz** in der VM
   (0 schwarze Pixel) -> Mesa toleriert, also kein VM-Repro.
4. Gewicht `grassTexW = 1` als Rote Ausgabe: rotes Band sichtbar (y=90..120 bei
   640x360) -> Mapping/Band in Ordnung, UV-Hypothese aus Runde 11 war falsch.
5. `/tmp/mipinc.c` (eigener GL-Test, 64x64, eine Textur mit / eine ohne Mips):
   `INCOMPLETT 1:1 = 0 0 0`, `INCOMPLETT minifiziert = 0 0 0`,
   `COMPLETT = 200 255 60`, **GL-Err 0x0 in allen Fällen**. llvmpipe ist also
   doch streng -> incomplett ist ein echter Defekt, er war nur nicht der Grund
   des VM-Bildes.
6. Beweis im Demo: Mip-Level 1 von `grassTex` an ein FBO hängen ->
   `status=0x8cd6` (`GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT`). Nach dem Fix:
   `status=0x8cd5` (`GL_FRAMEBUFFER_COMPLETE`).

### FIX (main.c, `grass_capture`)
Vor `glGenerateMipmap`: `glBindFramebuffer(GL_FRAMEBUFFER, 0)` +
`glBindTexture(GL_TEXTURE_2D, grassTex)`. (FBO weg, damit die Textur nicht
während der Mip-Erzeugung als Farb-Anhänger hängt.)

### FIX 2 (fragment.glsl, `terrainColor`)
Der `texture()`-Fetch lag in einem `if (grassTexW > 0.002)` — implizite
Ableitungen (Mip-Wahl) sind in **nicht-uniformem Control-Flow undefiniert**.
Jetzt ausserhalb jeder Bedingung geholt und per `mix(grass, ta, grassTexW)`
gemischt (w=0 ist identisch zum alten Weg, der Fetch ist billiger als ein
defekter Mip). Das ist die zweite host-relevante Fehlerquelle.

### Verifikation
- Mip-Probe nach dem Fix: `0x8cd5` = COMPLETE (vorher `0x8cd6`).
- `/tmp/tex1.sh` (1280x720, 6 s vorwärts, Screenshot TEX an / aus / an): läuft
durch, Einfang 37.194 Halme im 16-m-Feld (145 Halme/m²), Textur-PPM mean
(72,147,31), 0 schwarze Pixel im Bild.
- **Visuelle Bestätigung auf dem Host nötig** (NVIDIA war der schwarze Fall).

### Neue Mess-Infrastruktur
- `/tmp/mipinc.c` — Mipmap-Vollständigkeits-Repro (baut mit
  `-Ithird_party/include -l:libGL.so.1 -l:libX11.so.6`, läuft ohne Fenster-Show).
- `/tmp/tex1.sh` —Screenshot-Serie TEX an/aus/an bei 1280x720.
- `/tmp/an.py` — Bildstatistik (Mittelwerte, schwarze Pixel, Zeilenprofil).
- **Wichtig:** dieses Modell kann keine Bilder sehen -> visuell nur numerisch
  (Mittelwerte/Differenzen) plus Augen auf dem Host.

## Session 2026-10-07, Runde 11 (Dichte-Neubau, Winkeldünnung, ferne Gras-Textur)


### `1884fb0` — neues Dichteprofil
`dens(r) = (Sockel + Boost) · Ausschnitt`
- **Sockel** `1/(1+(r/R1)^p)`, R1=27, p=3 (`DENS_P`). Der 1/r²-artige Schwanz ist
  der Grund, warum Reichweite fast nichts kostet:
  `N = π·n0·R1²·ln(1+(R/R1)²)` — **logarithmisch** in R.
- **Boost** `3/(1+(r/2)⁴)` = 4× im engsten Nahfeld, bei 4 m praktisch weg
  (r=1: 3,8× · r=2: 2,5× · r=3: 1,5× · r=5: 1,04×), kostet nur **0,6 %** des
  Halmebudgets. Genau so vom Nutzer gefordert.
- **Ausschnitt** `(1−smoothstep(md−12, md, r))` — `DENS_EDGEW` **muss** das selbe
  12-m-Band sein wie die LOD-Dämpfung in `grass.vert`. Mein erster Versuch
  `(1−smoothstep(0.6·md, md, r))` legte die Generierungskante auf 99 m, die
  LOD-Dämpfung begann bei 98 m -> Kante bei **lod ≈ 1,0**, also genau die harte,
  voll sichtbare Graskante, die Runde 7 behoben hatte. Durch Messen gefunden.
- `nb`-Cap 32 -> `GRASS_NBMAX 128` (uint8_t-Zellen möglich bis 255).
- **`CT` 700 -> 1000**: das Build-Quadrat ist 2·g_half breit; bei md=110 sind das
  717 Zellen, 700 hätten das Feld bei 105 m abgeschnitten.
- `grassMaxDist` 80 -> 110, Slider 40..150.
- Radial (Halme pro Band, md=110 gegen md=80 alt): 1–2 m 250→787, 10–20 m
  116983→100429, 40–60 m 221558→83531, 60–80 m 143243→60176, 80–100 m 0→45886,
  100–110 m 0→4533. Gesamt 849562 -> 551268.
- Kosten: Initial-Build 933 -> 920 ms, POS-Build 1 m 157 -> 186 ms
  (p=2 hatte 225 ms). Mehr Zellen im Walk-Quadrat (445² -> 717²).

### `9a9cfba` — drei Dinge
1. **Blickwinkel-Dünnung (U, standardmäßig AN) — der fehlende Faktor.**
   Ein senkrechter Halm wird **nicht** foerziert, eine Wiese schon (Faktor sin θ).
   `Coverage = n·h·w / sin θ` -> konstante Pixeldichte verlangt `n ∝ sin θ`.
   Gemessen ohne Modulation:
   `Blick −2°: Coverage 9,40 / Halme pro Pixel 0,310` · `−10°: 8,11 / 0,221`
   · `−89°: 4,89 / 0,001`. Mit `n *= (sin θ / 0,35)^0,7`:
   **Coverage 5,19 / 5,18 / 5,13 / 4,51 / 4,89** — über alle Blickwinkel flach,
   senkrechter Blick bleibt zu 100 % erhalten. Umgesetzt im Vertex-Shader: der
   Schwellwert ist pro Halm stabil aus `iPos.xz` gehasht, weicher Übergang plus
   Cull darunter -> beim Drehen dünnen vorhandene Halme aus, es ploppen keine
   nach, und es ist **kein Rebuild** nötig.
2. **Treppen-Fix:** `dens_at` benutzte `(int)(dist*2.0f)` ohne Interpolation ->
   die Dichte war eine **Treppe mit 0,5-m-Stufen** (konzentrische Ringe). Jetzt
   linear interpoliert. (Frage des Nutzers, Antwort war: ja, es lief in Stufen.)
3. Sockel p=3. Wichtig zum Nachjustieren: **bei gleichem Budget** (R1 jeweils
   neu gelöst) bringt p=2→4 nur **8 %** weniger Halme in der Shimmer-Zone; mit
   **festem** R1 sinkt die Gesamtzahl von 0,97× auf 0,65×.

### AA-Diagnose: meine eigene Hypothese war falsch
Kein Sub-Pixel-Undersampling (0,03 Halme pro Pixel bei 40 m — Halme sind viel
größer als ein Pixel), sondern **Halmbreite**:
`w_px` = 8,2 (5 m) / 4,1 (10 m) / 2,3 (20 m) / 1,82 (30 m) / 1,67 (40 m) /
1,53 (60 m) / 1,36 (80 m) / 1,04 (110 m). Ab ~25 m ist jeder Halm eine
1–2-px-Rippe, deren MSAA-Abdeckung beim Kameramarsch flackert. Das ist das
Rauschen, das der Nutzer sieht.

### `17dcb49` — ferne Gras-Textur (Z-Taste, standardmäßig AN)
Idee vom Nutzer: die Engine fängt ein echtes Grasstück ein und nutzt es als
Oberfläche für entferntes Terrain. Kostet Laufzeit, **keine Datei-Bytes**.
- `main.c grass_capture()`: Ortho-Kamera, 1024×1024 RGBA8 + Depth, 16 m Feld,
  zeichnet alle Halme als **ALBEDO** (`uAlbedoOnly`: ohne Beleuchtung, Fog,
  Tonemap — das Terrain beleuchtet sie normal, sonst würden sie doppelt
  angewandt). Hintergrund = Terrain-Grasfarbe genau dort, damit die Lücken
  zwischen den Halmen zum Untergrund passen. Sucht einen grasigen Punkt
  (Wasser/Steilhang raus) in einer Spirale um die Kamera.
- **Regionengitter 48 m** -> pro Region neu eingefangen (`capRegion`).
- `fragment.glsl`: Tiling über Welt-XZ, hängt sich in die bestehende Gras-Maske
  (Fels/Sand/Schnee raus), Crossfade über `uGrassFar0..Far1`. Das prozedurale
  „Gras-Detail (Top-Down-Sicht)“ wird im selben Band mit `(1−grassTexW)`
  zurückgenommen, damit sich beides nicht überlagert.
- `uFarFade` im Gras-Shader = **dasselbe Band**, damit die Halme genau dort
  ausdünnen, wo die Textur übernimmt.
- **P schreibt zusätzlich `/tmp/64kdemo_grasstex.ppm`** — damit ist der Einfang
  ohne Bildurteil prüfbar. Das war der entscheidende Test.

### Zwei Erkenntnisse beim Messen, beide gegen meine Annahme
1. **Senkrechte Ortho-Aufnahme ist unbrauchbar:** nur **7,3 %** Bildabdeckung bei
   145 Halmen/m², weil von oben fast nur die Spitzen sichtbar sind (Struktur im
   Dump = Punkte, kein Streifenmuster). Fix: **schräge** Aufnahme (`capElev` 22°,
   auf Nutzeranstoß) -> **48 %** Abdeckung, 12.704 Farbwerte, Streifenstruktur.
   Der echte Blickwinkel auf fernen Grund ist winzig (2 m Kamerahöhe, 40 m
   Distanz = 3°); eine stark foerzierte Aufnahme wäre aber ein 16×1,4 m-Schmalband
   und kachelt sichtbar. 22° ist der Kompromiss; `capElev` ist eine Variable.
   Ein Scher-Hack (Höhe in die Bildebene falten) war gebaut und ist wieder raus —
   die schräge Kamera erledigt das physikalisch richtig.
2. **Depth-Renderbuffer:** mit `glRenderbufferStorageMultisample(target, 1, …)`
   blieb der FBO bei `0x8d56` (`GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT`). Mit dem
   regulären `glRenderbufferStorage` ist er komplett.

### Stummschaltbare Parameter
| wo | Wert | Wirkung |
|---|---|---|
| `capElev` (main.c) | 22° | Aufnahmewinkel der Ferntextur |
| `CAP_M` / `CAP_TEX` / `REGION_M` | 16 m / 1024 / 48 m | Feldgröße, Auflösung, Region |
| `grassTexFar0/Far1` | 25 / 45 m | Übergabe Geometrie -> Textur |
| `DENS_R1 / DENS_RB / DENS_P / DENS_EDGEW` | 27 / 2 / 3 / 12 | Dichteprofil |
| `grassAngS0 / grassAngQ` | 0,35 / 0,7 | Blickwinkel-Dünnung |
| `grassMaxDist` | 110 (Slider 40..150) | Sichtweite |

### Tasten (Debug-Zweig)
C Farbmodus · J Depth-Write · K Dichte-Kante · L Fade-Dauer · Y Alpha-to-Coverage ·
I sortierte Reihenfolge · U Blickwinkel-Dünnung · Z ferne Gras-Textur ·
G Freeze · V Nur-Gras · M MSAA · B Dump · P Screenshot **+ Gras-Textur-PPM**
HUD: `DW± EDGE± CAT A2C SORT WINK TEX FRZ NUR-GRAS FADE`

### Regression (nach jedem Schritt gelaufen, alle grün)
0 Pops, Fade-Bilanz am Swap exakt 0,00 %, Idempotenz exakt (10 Builds am selben
Zentrum = 0 Änderung), Hin-und-zurück 0,005 %, 15/512 Gruppen, 1 Duplikat
(vorbestehende Hash-Kollision). Tests: `/tmp/test_grp.c`, `test_idem.c`,
`test_there.c`, `test_densprof.c` (Radialprofil + Build-Zeit), `test_zhide.c`
(Issue-A-Beweis), `timing.c` (alt/neu), `/tmp/densdesign*.py`, `/tmp/aa2.py`,
`/tmp/winkel.py` (Rechnungen).

## Session 2026-10-06, Runde 10 (URSACHE GEFUNDEN: Depth-Write halbtransparenter Halme)

**Nutzer-Host-Test mit J (Depth-Write) hat die Ursache geklaert.** Mit `DW-`
ist der Sprung weg, mit Depth-Write sichtbar. Mechanismus: ein sterbender Halm
ist halbtransparent, **schreibt aber Tiefe fuer alle Samples** und verdeckt damit
eingespielte Halme dahinter. Faellt er beim harten Cull (`f < 0.004`) weg, wird
der verdeckte Halm **schlagartig** sichtbar — genau das beobachtete
"ploppt als gruen instant auf".

Gemessen (/tmp/test_zhide.c, 4-m-Rueckwaertshop, t = Swap + 0,95*Fade):
```
im Bild: 24214 sterbende, 176066 eingespielte Halme
eingespielte Halme hinter einem naeheren sterbenden: 98278 = 55,8 %
```
Das erklaert alle frueheren Befunde: Buffer-Daten waren sauber (sie sind es),
der Effekt ist an der Graskante am staerksten (dort sitzen die sterbenden),
er kommt erst NACH dem Ausblenden (der Cull ist der Ausloeser), und der
Dichte-Slider ist sauber (dort sterben alle gleichzeitig, dahinter liegt nichts
Eingespieltes).

**Fix: Alpha-to-Coverage (Y, standardmaessig AN).** Alpha wird in eine
MSAA-Sample-Maske uebersetzt; ein Halm schreibt Tiefe nur noch fuer abgedeckte
Samples. Bei Alpha ~0,05 deckt er bei 8x kein Sample ab -> verdeckt nichts.
Wirkt nur mit MSAA (Standard 4x, M-Taste 4x/8x/aus).

Wichtige Details, beide vom Nutzer korrigiert:
- A2C **plus** SRC_ALPHA-Blending daempft doppelt (Alpha 0,5 bei 4x -> 0,25).
  Deshalb: A2C an -> Blending aus, A2C aus -> Blending an.
- Die "5 Alpha-Stufen bei 4x" sind pro Sample gemeint und **nicht sichtbar**:
  die Sample-Auswahl ist gedithert und variiert mit der Fensterposition, nach
  dem MSAA-Resolve ergibt das eine kontinuierliche Dichte. Der sqrt(alpha)-
  Trick zur Linearisierung ist nach Nutzerbefund unnoetig.

### Sortierte Zeichenreihenfolge (I-Taste, zum Vergleich)
Zellen laufen beim Build fern->nah entlang der Blickrichtung vom Puffer
(512 Slabs, Counting-Sort). Zell-Bloeecke bleiben zusammenhaengend,
cellOff/cellCnt bleiben gueltig. Kernpunkt: Sortieren nach Projektion auf eine
FESTE Achse ist translationsinvariant — Kamerabewegung aendert die Reihenfolge
ueberhaupt nicht, nur Drehung zwischen zwei Builds (0,3..1 s) verwischt sie leicht.

Gemessen (/tmp/test_order3.c, 5 Hops a 4m):
| | Diamond-Order | sortiert |
|---|---|---|
| mittl. Abweichung von idealer Reihenfolge | **31,77 m** | **0,12 m** |
| Chunk-Verletzungen (Blick +x) | 11/31 | 0/31 |
| Chunk-Verletzungen (Blick diagonal) | 16/31 | 0/31 |

Die ~50 % "falschen Nachbarpaare" bleiben in beiden Faellen: innerhalb eines
0,3-m-Slabs ist die Reihenfolge egal (Tiefenunterschied < Slab-Breite). Dieses
Mass war vorher irrefuehrend — aussagekraeftig ist die Abweichung von der
idealen Ordnung.

**ENTSCHEIDUNG (Nutzer, nach Host-Test auf dem Zweig mit A2C):** der Effekt der
Sortierung ist 'mild bis unsichtbar'. **A2C genuegt vollauf.** Die sortierte
Walk-Ordnung bleibt als Debug-Schalter (I) in diesem Zweig, kommt aber **nicht
nach main**. Damit ist Issue A abgeschlossen.

**Debug-Schalter gesamt:** C Farbmodus, J Depth-Write, K Dichte-Kante,
L Fade-Dauer (1/2/10/30 s), Y Alpha-to-Coverage, I sortierte Reihenfolge,
G Freeze, V Nur-Gras, M MSAA, B Dump, P Screenshot.

## Session 2026-10-06, Runde 9 (Fade-GRUPPEN + Intervall-Markierung gebaut)

Auf Nutzer-Idee hin umgesetzt: Birth-/Death-Zeit ist ein Fakt **pro Build**, nicht
pro Halm, und die Halme einer Zelle sind eine ueber Builds stabile Teilmenge.
Beides war im Code belegbar (`grass_build_finish` setzte fuer ALLE Halme eines
Builds dieselbe Zeit), also waren die Pro-Halm-Floats redundant.

### Was jetzt anders ist
| vorher | jetzt |
|---|---|
| 12 Floats/Halm (birth, death, b-Index) | **10 Floats/Halm** (nur Gruppen-Indizes) |
| `copiedMask` (uint32/Zelle) + `popcount` | entfaellt — alles Intervalle |
| `alive < nb` (Pufferreihenfolge) vs. `nNew` nach b-Index | eine Reihenfolge, Widerspruch weg |
| Zell-Block unsortiert | kanonisch **[lebend][sterbend]** |
| Generierung scannt b < nb neu | setzt bei `cellNextB` fort (Intervall) |
| nb im Kopierpfad am Zellzentrum, in der Generierung am Punkt px,pz | **eine** Formel, immer Zellzentrum |
| Finish-Vollpass ueber alle Halme | entfaellt (O(Gruppen)) |
| Death-Time als `-1`-Platzhalter + Swap-Sonderlogik | nicht mehr darstellbar (Gruppe entsteht am Swap) |

Halm speichert beide Indizes gepackt in einem Float: `wert = birthGrp +
deathGrp * GRASS_NGRP` (2*9 Bit, exakt in einem float). Zellentabelle hat jetzt
`cellOff`, `cellCnt`, `cellAlive`, `cellNextB`.

### Messergebnisse (alt -> neu)
- **Build-Zeit 1m-Shift: 157 -> 139 ms (−11 %)**. Initial-Build gleich (939/937 ms).
- **Puffer: 43,8 -> 36,6 MB (−16 %)**, VBO-Upload pro Build entsprechend.
- **Fade-Bilanz am Swap: +0,1/+0,4/+1,5 % -> exakt 0,00 %** (Hop 1/4/12 m).
- **Idempotenz: 10 POS-Builds am selben Zentrum = exakt 0 Aenderung, 0 sterbend**
  (vorher 12–350 Halme Restchurn). Hin-und-zuruck-Fahrt (60 Builds): 853.522 ->
  853.600, also auf 0,009 % zurueck beim Ausgangswert (vorher +516 = 0,06 %).
- **Keine Duplikate**: 2 (die vorbestehende Hash-Kollision), wie vorher.
- **Kein Pop**: 0 am Swap geborene Halme mit Fade > 0,02.
- Jede bestockte Zelle hat exakt `alive == nb` (Histogramm 89557/0/0/0/0/0).
- Halmzahl +0,5 % gegen den alten Code (Zellen fuellen sich jetzt auf exakt nb).

### Zwei Fehler, die ich selbst eingebaut und gemessen habe
1. **Kombinierte (birth,death)-Gruppe pro Halm** — haette pro (Birth-Gruppe x
   Build) eine Gruppe gebraucht und waechst kombinatorisch: gemessen 24 neue
   Gruppen pro Build, bei 30 s Fade ~2400 gleichzeitig live. Die Tabellen-groesse
   skalierte mit dem Bedarf, nicht mit der Groesse (255/256, dann 511/512).
   Fix: getrennte Indizes, pro Build genau EINE Death-Gruppe. Jetzt 15 Gruppen
   bei 1 s Fade, 143/512 bei 30 s.
2. **Python-Slice hat zu viel ersetzt** (g_half/dens_at/build_begin geloescht).
   Fing der Build mit `dens_at undeclared`. Aus Backup wiederhergestellt und die
   Aenderung gezielt mit Edit-Werkzeug gemacht.

### Nicht verwechselt werden mit dem Symptom
Das Halmwachstum entlang einer Fahrt (853k -> ~1,1M bei cx=90) ist **kein Leck**:
Rueckfahrt zum Startpunkt liefert den Ausgangswert zurueck, und der alte Code
zeigt dasselbe (849k -> 996k). Es ist Terrain-Variation entlang der Strecke.
Der Umbau aendert nichts daran, WELCHE Halme existieren — er macht das System
kleiner, schneller und robuster. Issue A (sichtbarer Sprung) bleibt offen.

### Testdateien
/tmp/test_grp.c (Duplikate, Pops, Bilanz, Gruppen), /tmp/test_grplive.c
(referenzierte Gruppen 1s/30s), /tmp/test_there.c (hin/zurueck), /tmp/timing.c
(alt vs. neu), /tmp/test_idem.c (Idempotenz), /tmp/oldver/ (alter Code zum
Vergleich). Alter Stand auch in /tmp/backup_runde9/.

## Session 2026-10-06, Runde 8 (Issue A: Ursache eingegrenzt, Debug-Werkzeug gebaut)

### Kernbefund: das Dichteprofil hängt am BUILD-ZENTRUM, nicht an der Kamera
`nb = 32·dens_at(dist)` mit `dens_at` = Distanz zum Build-Zentrum. Jedes Mal,
wenn das Zentrum hüpft, ändert sich die Ziel-Halmzahl **in jeder Zelle** — nicht
nur an einer Kante. Es ist eine ganze Treppe von Dichtestufen (Nutzer:
„das sind lauter Dichtekanten"), und die wandert als Ganzes.

Gemessen (/tmp/test_nbshift.c, lebende Halme pro Zelle, eingeschwommen):

| Hop | Zellen geändert | mittl. \|Δnb\| | max Δ |
|---|---|---|---|
| 1 m | 20,7 % | 1,11 | +3 |
| 4 m | 59,0 % | 1,56 | +6 |
| 12 m | **86,0 %** | 3,08 | +14 |

Das erklärt die Nutzerbeobachtung exakt: kleiner Hop → nur an der Kante sichtbar
(Steigung des Dichte-Falloffs dort am größten), großer Hop → fast überall.

### Die Buffer- und Fade-Daten sind sauber — kein Pop in den Zahlen
- /tmp/test_freeze_pop.c: **0** Halme erscheinen mit vollem Alpha ohne Vorgänger
  an derselben Stelle (Hops 1/7/17/34/57 m, exakter Float-Vergleich).
- /tmp/test_gone.c: **0** voll sichtbare alte Halme verschwinden ohne Fade —
  alle 849562 sind im neuen Buffer wieder da (lebendig oder sterbend).
- /tmp/test_noop.c: ein zweiter POS-Build am SELBEN Zentrum ist nahezu ein
  No-Op: 12–350 Halme (≈0,03 %) oszillieren noch.
- /tmp/test_bilanz.c (summierte sichtbare Alpha, zeitlich aufgelöst, OHNE Grafik):
  Hop 1 m **+0,1 %**, 4 m **+0,4 %**, 12 m **+1,5 %** — der Crossfade ist global
  balanciert. Räumlicher Saldo bei 12 m: nah **+23218**, mittel +10541, fern **−20618**.

### Kleiner echter Defekt (nicht behoben): Asymmetrie Copy- vs. Gen-Auswahl
Der Kopierloop entscheidet „bleibt am Leben" über die **Pufferreihenfolge**
(`else if (alive < nb)`), der Generierloop zählt aber über den **b-Index**
(`nNew = nb - popcount(copiedMask & want)`). Sobald die lebenden Halme einer
Zelle nicht exakt b = {0…nb−1} belegen, zählt nNew zu viele → Zelle wächst über
nb → im nächsten Build werden Überzählige getötet → Oszillieren. Korrekt wäre
`nNew = nb - alive`. Magnitude klein (0,03 %), **nicht** das Nutzersymptom.

### Zeichenreihenfolge: nur grob sortiert, und Depth-Write ist NIE aus
Zeichnet wird in 32 Chunks vom Puffer-ENDE nach vorn (main.c), Pufferordnung =
Diamond-Order um das Build-Zentrum.
Gemessen (/tmp/test_order2.c, Chunk-Mitteldistanz zur Kamera):
- 0 m Drift: **0/31** Verstöße (9,7 → 67,3 m monoton) — Ordnung gut.
- 20 m Drift: 1/31. 40 m Drift: **9/31**, Nahhälfte praktisch unsortiert.
- **Innerhalb** eines Chunks (26549 Instanzen): ~50 % der Nachbarpaare falsch,
  Größenordnung bis **24 m**.
`glDepthMask` wird in main.c **nirgends** auf `GL_FALSE` gesetzt — jeder Halm
schreibt Tiefe, auch mit Alpha ≈ 0 (und der Vertex-Shader cullt hart bei
`f < 0.004`). Ein näherer Halm, der innerhalb eines Chunks vor einem ferneren
gezeichnet wird, cullt den ferneren komplett; am Swap ändert sich die Sortierung
→ verdeckte Halme werden schlagartig sichtbar. **Hauptverdacht, unbestätigt.**

### Ausgeschlossen: Build-Rate begrenzen („kein Build während eines Crossfades")
Bei 13 m/s bzw. 34 m/s Kamera (main.c:1108) ergäbe ein Build alle 2 s Hops von
26 m bzw. 68 m. Gemessen (/tmp/test_buildrate_order.c): Zellen geändert
**94,1 % / 95,6 %**, mittl. \|Δnb\| 5,47 / 8,79, max +26 / +45. Zusätzlich läge
das Feldzentrum 26–68 m hinter der Kamera → Gras würde **vor** der Kamera
verschwinden. Idee verworfen.

### Debug-Werkzeug (Branch `dbg-gras-fade`)
- **C** — Kategorie als **Albedo** (gelb = läuft aus, cyan = läuft ein), und zwar
  nur **während** ein Fade läuft; ist er durch, normales Gras. Beleuchtung,
  Transluzenz, Specular, Fog, Tonemapping und Alpha-Blending bleiben unverändert.
- **J** — Depth-Write im Gras-Pass an/aus (A/B-Test der Reihenfolge-Hypothese).
- **K** — Dichte-Kante an/aus (`grassSetDensCut`, CUT 0.01 vs. 0 = kein Abschneiden).
- **L** — Fade-Dauer 1/2/10/30 s; `grassFadeDur` ist eine Größe für CPU **und**
  Shader (uFadeDur), sonst werden sterbende Halme aus dem Puffer geworfen,
  während sie noch einblenden.
- **Fade-Dauer Standard 1 s** (vorher 2 s). Slider-OUT→IN-Wartezeit nutzt jetzt
  ebenfalls `grassFadeDur` statt fest 2 s.
- **Slider-UI** blendet aus, solange die linke Maustaste gehalten wird (außer beim
  Ziehen eines Sliders); Ausblendrate beim Look 0,5 s. Zustandsmaschine isoliert
  getestet (/tmp/test_sliderfade.c): Look → 0, Drag → bleibt 1.

### Messfallen — alle bezahlt, bitte nicht nochmal
- **G-Kanal-Screenshots sind als Pop-Messung unbrauchbar:** `sizeScale =
  1+1,8·(1−dens)` skaliert die Halmgröße mit Kameradistanz; reine Kamerabewegung
  ändert die bedeckte Pixelmenge um +50 %.
- **`lod` ist NICHT in der Alpha** (`vFade = f`), es skaliert nur die Halm**höhe**.
  Ferne Halme sind flache Streifen mit voller Alpha.
- **Rot ist im Nur-Gras-Modus unsichtbar:** Hintergrund ist Magenta, Rot über
  Magenta gemischt bleibt Magenta. Deshalb gelb/cyan.
- **VM läuft mit 1–2 FPS:** ein 2-s-Fade ist dort zeitlich nicht auflösbar, und
  Screenshot-Auslösung über eine Log-Pipeline verfehlt den Zeitpunkt systematisch.
  Abhilfe: Fade auf 10–30 s strecken (L) **oder** Bilanz auf der CPU rechnen.
- **Synthetische Mausklicks** (`xdotool mousedown`) erreichen das Demo-Fenster
  nicht → UI-Änderungen sind in der VM nicht pixelprüfbar.
- **Nachbarpaar-Vergleich der Instanzreihenfolge ist reines Rauschen** (Nachbarn
  liegen in derselben Zelle, Distanzunterschied cm) → auf Chunk-Ebene messen.

### Architektur-Vorschläge (vom Nutzer angestoßen, noch nicht umgesetzt)
1. **Zellen sortiert nach gültigem b speichern → Intervall-Markierung** statt
   copiedMask. Vorbedingung ist erfüllt: alle Verwerfungen (Zellhöhe, Steilhang,
   Gras-Flecken, Wasser pro Halm) sind deterministisch in (Zelle, b) bei
   statischem Terrain → die Folge gültiger b ist über Builds stabil, und „die
   ersten nb Halme der Zelle" ist eine stabile, flächig streuende Teilmenge.
   Entfernt copiedMask, das alive-Zählen und `d[11]` (3,4 MB nur dafür).
2. **Fade-Gruppen statt per-Halm-Timer:** Uniform-Array mit (birth, death) pro
   Build, pro Halm nur ein Gruppen-Index. 11→9 Floats, die Finish-Schleife über
   850k Halme entfällt. Grenze: sterbende Halme dürfen nicht wiederauferstehen
   (Alpha-Sprung) → Regel: nicht erzeugen, solange die Position belegt ist.
3. **Persistenter Buffer mit In-Place-Zell-Updates + glBufferSubData** statt
   Doppelbuffer-Vollkopie. Aktuell: 12 Floats = 48 Byte/Halm, 850k Halme =
   **40,8 MB Kopie + 40,8 MB Upload pro Build**; nötig wäre nur die Sichel.
4. **Dichte-Begrenzung pro Build:** `nb_used = clamp(nb_ziel, A−1, A+1)` → ein
   großer Hop verteilt sich auf ~14 Builds à 1 Halm/Zelle. Braucht Settle-Loop.
   (Build-Sperre während Fade ist dagegen falsch, siehe oben.)

### Commits dieser Runde
- `main`: 666de2c (Kanten-Fix CUT 0.09→0.01) — **vom Nutzer nicht bestätigt, Sprung bleibt**
- `dbg-gras-fade`: 32fd4f4 (Debug-Zweig), 555fadc (Farbmodus als Schalter, normale
  Grafik zurück), dd30782 (L-Zyklus mit 1 s, Slider bei Maushalt aus), + Fade-Standard 1 s

## Session 2026-10-06, Runde 6 (Issue A Teil 2: Overlap-Bug FIXED)

### FIXED: Overlap-Bug im Copy/Gen-Pfad (Issue A Teil 2)
- **Wurzel (zwei Mechanismen, per exaktem Float-Vergleich belegt, s. Issue-A-
  Sektion):** (1) onb-Off-by-one (Zellen- vs. Patch-Zentrum → ±1, bereits
  nach einem Dichte-Wechsel 331 Duplikate), (2) sterbende Halme werden
  kopiert UND ab bStart=onb neu generiert (mit dichten Builds 63.790
  Duplikate).
- **Fix (grass_cpu.c, scene.h):** b-Index pro Halm speichern (d[11],
  GRASS_FLOATS 11→12, CPU-nur). Copy-Loop baut 32-Bit-Maske `copiedMask`
  der tatsächlich kopierten b's; Gen-Loop erzeugt nur b < nb, die NICHT
  kopiert wurden (statt bStart=onb). onb-Berechnung + old_bcx/old_bcz
  entfernt (überflüssig). VAO-Stride + VBO-Upload nutzen GRASS_FLOATS
  automatisch → keine main.c-Änderung.
- **Verifiziert:** Unit-Test /tmp/test_overlap4.c (exakter Float-Vergleich,
  3-Phasen-Szenario): Duplikate 331/983/63790 → 1 (nur noch vorbestehende
  Hash-Kollision). Regressionstests test_edge_pop (0 Pops), test_death
  (6 Szenarien OK), test_1m (1m ~140ms) alle OK. Binary-Größe unverändert
  (72.248 B). VM-Pixeltest Hin-und-Her-Fahrt (/tmp/issueA2.sh vs
  /tmp/issueA2_orig.sh): G-Kanal + Summe-ShaderFade identisch zum Original
  (kein Regression). **Host-Test vom Nutzer ausstehend.**
- **NEU Befund (vorbestehend, eigenständig):** Hash-Kollision in Zelle
  (83,9): b=11 und b=12 liefern exakt dasselbe (bx,bz) (doppelte
  c_hash12-Kollision). 1 Halm von 587k doppelt, im Original schon da,
  visuell vernachlässigbar. Fix = Hash-Funktion ändern (betrifft alle
  Halme) — optional, eigener Schritt.
- **Lehre:** Der 1mm-Test (560 Duplikate) vermischte echte Overlaps mit
  Raster-Effekten. Exakter Float-Vergleich (Bitmuster) trennt sie sauber:
  echte Overlaps = exakt gleiche Floats (deterministisch aus (ix,iz,b)).
  Vor der Fix-Theorie erst die Test-Präzision erhöhen.

### Test-Infrastruktur (ergänzt, /tmp)
- /tmp/test_overlap4.c: exakter Float-Vergleich (Bitmuster), 3-Phasen-
  Szenario mit 3s- und 0.3s-Build-Abstand (Fade fertig vs. läuft).
- /tmp/test_phase1.c: Duplikat-Check im reinen Generierungs-Buffer (Phase 1).
- /tmp/test_dupdetail.c: b-Indizes + Zelle der duplizierten Position.
- /tmp/grass_cpu_orig.c + /tmp/scene_orig.h: Original für Vorher/Nachher.
- /tmp/issueA2.sh + /tmp/issueA2_orig.sh: VM-Pixeltest Hin-und-Her-Fahrt
  (G-Kanal-Alpha + Summe-ShaderFade), mit neuem/Original-Binary.

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
- `GRASS_FLOATS=12` (d[9]=Birth, d[10]=Death, d[11]=b-Index), `GRASS_CAP=5000000`,
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
- [x] **Ferne Krone duenner als nahe** — war −34 % Kronendeckung, jetzt −20 %
  (tier 0: `wantCards ezNSprig/3`, Zweigroehren weg; Farbe war schon vorher gleich)
- [ ] **Rebuild-Stottern**: Builder im Worker-Thread + Upload in Scheiben erledigt
  (Runde 26). Offen: Datenmenge — Karten sind 6 Eckpunkt-Instanzen a 13 Float
  (312 Byte/Karte); instanzierung waeren 52 Byte -> Upload und Build ~6x billiger
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
  GRASS_CAP 5M, GRASS_FLOATS 12
  (pos3, hoehe, breite, phase, schattierung, yaw, lean, birth, death, b)
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
- Caps: TREE_VERT_CAP 5200000, CROWN_CAP 900000, LEAF_CAP 2400000, GRASS_CAP 1500000
## Session 2026-10-09, Runde 32 (Baum-LOD neu aufgebaut: Karten = Basisstufe)

**Ausgangspunkt (Nutzer):** Laub blendet aus, bevor die andere Darstellung einblendet;
beim Annähern baut ein Baum sein Blattwerk Blatt für Blatt neu auf; ferne Kronen sind
überzeugende grüne Flecken nicht.

**Messfehler zuerst berichtigt:** die alten Deckungs-Zahlen waren mit Gras *und*
(bseit Runde 31) Schilf im Maskenbild — deshalb sah man ein "Loch" von 60–86 m, das es
nicht gab. Saubere Messbank: `DEMO_NOTERRAIN=1 DEMO_GRASSMD=0 DEMO_REEDPASS=0
DEMO_ONETREE=1 DEMO_TREEDIST=<m> DEMO_FREEZE=1`. Zusätzlich war `DEMO_NOBB0=1`
verdreht (setzte den Billboard auf 1) — alle Billboard-Vergleiche damit waren wertlos.

**Neue Regeln (CPU):** Karten gibt es in *jeder* Stufe und der Shader zeichnet sie ohne
Entfernungsschwelle — sie sind die Basis der Krone. Einzelblätter sind eine
Zusatzschicht, die im Band 0,9–1,1·R1 statistisch ein-/ausblendet, und sie werden bis
1,3·R1 gebaut, damit in diesem Band immer beide Schichten im Puffer liegen.
Schaltweiten R1 = 5×, R2 = 15× Wuchshöhe (vorher 10×/20×): ein 6-cm-Blatt ist ab ~50 m
unter einem Pixel, die Einzelblätter decken die Krone nur bis etwa 5× Wuchshöhe.

**Karten-Dichte:** `DEMO_CARDT2="<div> <scale>"` durchgemessen — wenige große Karten
schlagen viele kleine: div 3 / scale 2,0 statt div 2 / 1,25 hebt die Deckung von 38 %
auf 50 % und senkt die Instanzen von 287 194 auf 234 129.

**Deckung ein Baum (H 3,9 m, R1 20 m, R2 58 m), grün/bbox:**
10 m 41 % · 25 m 50 % · 40 m 52 % · 50 m 51 % · 58 m 60 % · 66 m 67 % · 86 m 73 % · 108 m 69 %
(vorher: 34 / 34 / 40 / 40 / — / — / 70 / 66 %). Größter Schritt zwischen zwei
Abständen jetzt +8 % (vorher Loch mit −70 %).

**Satzwechsel (Kameraweg 6 m/s, alle 0,5 s ein Bild):** Laubpixel 53698 → 51013 →
51010 → 50268 → 49820 → 49820 → 49616 → 49697 → 49794 → 49802, also kein Sprung beim
Rebuild. Der harte Cull des alten Satzes unter 30 m ist raus (war genau bei nahen
Bäumen ein Sprung); übergeblendet wird jetzt durchgängig über 0,9 s.

**Caps erhöht statt Geometrie zu überspringen** (Nutzerregel): TREE_VERT_CAP
3,8 M → 5,2 M, CROWN_CAP 700 k → 900 k, LEAF_CAP 1,6 M → 2,4 M. Im Stress-Szenario
`DEMO_LODRSCALE=2` fielen vorher 90 Bäume komplett raus. Selbsttest wieder grün.

**Offen nach dieser Runde:** FPS-Einbruch auf Taste `1` (Billboard aus) ist in der VM
nicht messbar; erklärt ist er durch +246 535 Karten-Instanzen in Stufe 0. Braucht eine
Draw-Call-/Instanzen-Statistik auf dem GPU-Host.

## Regelwerk Baum-LOD (Runde 38, vom Nutzer freigegeben — verbindlich)

Zwei getrennte Entscheidungen:
- **Existenz (Build):** entscheidet nur, welche Geometrie im Puffer liegt. Grosszuegig
  und immer jenseits der aeussersten Blende, damit kein Baum Geometrie verliert,
  waehrend sein Gewicht > 0 ist.
- **Sichtbarkeit (Shader):** ausschliesslich gewicht(d) der Kameradistanz, stetig.

Gewichte (H = Wuchshoehe, R1 = clamp(5H,20,60) m, R2 = clamp(15H,40,120) m).
Zwei sich ablösende Schichten benutzen dasselbe Band und sind komplementaer
(w und 1-w); mit echtem Alpha ist die Summe ueberall 1 — kein Deckungsloch,
keine Doppelung.

| Schicht | d klein | Band | d gross |
|---|---|---|---|
| Einzelblaetter | 1 | 0,90-1,10 R1 -> 0 | 0 |
| Karten | 0 | 0,90-1,10 R1 -> 1 | 1 bis 0,90 R2, dann 0,90-1,10 R2 -> 0 |
| Ganzbaum-Billboard | 0 | 0,90-1,10 R2 -> 1 | 1 |
| Geruest (Stamm+Aeste) | 1 | 0,90-1,10 R2 -> 0 | 0 |

Existenz-Regeln:
- Einzelblaetter gebaut bis 1,15 R1 + 20 m (Rand deckt den Nachlauf des Builds ab)
- Karten: EINE Dichte und EINE Skala fuer alle Stufen (div 3, Skala 2,00)
- Billboard: fuer alle Bäume
- Aeste: bis 1,15 R2, danach nur Stamm (ausserhalb jeder Blende, also unsichtbar)

Gemessen nach Umstellung: Karten 234 223 -> 446 918 (Cap 900 000), Geruest
1 179 702 -> 1 374 750 (Cap 5 200 000), Blaetter 507 870 -> 642 330
(Cap 2 400 000), 0 Cap-Treffer, Puffer 95 -> 121 MB pro Satz.

Fehler dieser Runde: fuer Attribut 4 (R2 pro Geruest-Vertex) fehlte
glEnableVertexAttribArray -> aR2 = Generic-Attribut 0 -> smoothstep(0,0,d)=1 ->
komplettes Geruest weggecullt. Diagnose: Geruest zwangsweise rot gezeichnet,
0 rote Pixel = gar keine Rasterisierung.

## Ein-Baum-Szene DEMO_ONE (Runde 41) — Werkzeug gegen Rebuild-Sprünge

`DEMO_ONE=1` zeichnet genau EINEN Baum (kein Terrain, kein Gras, kein Schilf)
und die Kamera springt an den Fokusbaum (`DEMO_TREEDIST=<m>`). Pro Frame kommt
eine CSV-Zeile:

  EB,t,camX,camZ,dCam,dBuild,tier,R1,R2,Geruest,Karten,Blaetter,leafW,cardW,bbW,meshW,fade,set,up

und bei jeder Änderung der Build-Entscheidung eine EB-STUFENWECHSEL-Zeile.

Gemessener Ist-Zustand VOR der Korrektur (ein Baum, H ca. 6,8 m, R1 34 m, R2 101 m):

| dCam | tier | Geruest | Karten | Blätter | mesh-Gewicht |
|---|---|---|---|---|---|
| 10–30 m | 3 | 9 993 | 841 | 12 600 | 1,00 |
| 40–50 m | 2 | **4 191** | 841 | 12 600 | 1,00 |
| 60–90 m | 2 | 4 191 | 841 | 0 | 1,00 |
| 110 m | 0 | 3 753 | 841 | 0 | 0,01 |
| 130 m | 0 | 45 | 841 | 0 | 0,00 |

Der Netzwechsel bei 36 m (tier 3 -> 2) passierte bei Mesh-Gewicht 1,00 — die
Mesh-Blende liegt erst bei 0,9..1,1 R2 (91..111 m). Ursache in ez_grow:
`stride = tier>=3 ? 1 : ...` und `G`-Reduktion fuer tier 2/0. Jetzt
entfernungsabhaengig (volles Netz bis 1,15 R2). Nachher: Geruest 9 993 bei
10..110 m, erst bei 130 m (Gewicht 0) 45 Vertices.
Kosten im Vollwald: Geruest 4 342 632 von 5 200 000, 0 Cap-Treffer,
0 uebersprungene Baeume.

## LOD-Kern und kanonischer Anker (2026-10-08, Strategie A + B)

Nutzer: „die LOD-Generierung und der Fade sind immer noch nicht synchron … wir
brauchen eine wirklich wasserdichte Debug-Strategie".

**Gefundene Ursache (gemessen, nicht vermutet):** dieselbe Frage „wie fern ist
dieser Baum" wurde an sechs Stellen mit vier verschiedenen Ankerpunkten
beantwortet — Rebuild an camTgt (main.c), Stufe an der Kamera zum Lotpunkt
(trees_cpu.c), Existenz am eingefrorenen Build-Abstand, Blatt-Fade an der
Blattposition, Karten-Fade an der Kartenmitte, Geruest-Fade am einzelnen Vertex.
Die Baelen waren formal komplementaer, bekamen aber verschiedene Argumente,
konnten sich also nie zu 1 ergaenzen.

**A — ein kanonischer Abstand pro Baum.** Der Lotpunkt des Baums liegt in jeder
Instanz: TREE_FLOATS 13 -> 15, CROWN_FLOATS 12 -> 14, LEAF_FLOATS 11 -> 13
(jeweils +anchorX/anchorZ). leaf.vert, crown.vert und tree.vert rechnen den
LOD-Abstand als length(anchor - uCamPos.xz).

**B — Kern als reine Funktion + Wirtstest ohne GL.** src/tree_lod.c/.h
(Schaltweiten, Schichtgewichte, Existenz-Schnitte, Stufen-Hysterese) und
src/shaders/lod.glsl als GLSL-Seite; die drei Baum-Shader binden lod.glsl per
#include ein (tools/gen_shaders.sh bettet sie ein). `make test-lod` faehrt
synthetische Kamerapfade: **7418 Pruefungen, 0 Fehler** — Komplementaeritaet
(|Summe-1| = 0), Stetigkeit gegen die analytische smoothstep-Schranke 1,5/Bandbreite,
Existenz ausserhalb der Baelen, Hysterese gegen Oszillieren, und dass GLSL und C
dieselben Bandzahlen benutzen.

**Radiale Messfahrt (neu: DEMO_APPROACH=<m/s>).** DEMO_WALK war ein fester Vektor
und fuhr seitlich am Baum vorbei — die hoechste Detailstufe wurde nie angefahren.
Ein Baum, H 6,8 m, R1 34 m, R2 101 m, Kamera 140 m -> 2,6 m:

| Groesse | Messwert |
|---|---|
| Stufenfolge | 0 -> 2 -> 3 (monoton, tier 1 unbenutzt) |
| groesstes Deckungsloch \|Blatt+Karte+Billboard-1\| | 0,0000 in jedem Frame |
| max \|dw/dd\| Blatt / Karte | 0,121 pro m (Schranke 0,221) |
| max \|dw/dd\| Billboard / Geruest | 0,072 pro m (Schranke 0,074) |
| Geometrie-Aenderungen | Geruest 1x, Blätter 1x, Karten 0x |
| Geruest 45 -> 7 362 bei 124,5 m | Geruest-Gewicht 0,000 (Blende 91..111 m) |
| Blätter 0 -> 12 600 bei 62,6 m | Blatt-Gewicht 0,000 (Blende 31..37 m) |

Geometrie taucht also ausschliesslich dort auf, wo sie unsichtbar ist.

**Schwarze Rechtecke:** gleicher Test (Normalszene, Impostor ueber Taste 1 an):
alter Bau 56 671 dunkle Pixel, neuer Bau 0. Ohne Impostor ebenfalls 0.

## Ein-Baum-LOD-Test DEMO_LODTEST und der Satz-Wechsel (2026-10-08)

Nutzer: „ich brauche einen Test, bei dem die engine nur den einen Baum und sonst
nix zeichnet und alle relevanten Schritte sauber loggt … deine Kamerafahrt fährt
übrigens viel zu schnell".

`DEMO_LODTEST="dStart dEnd step"`: ein Baum, kein Terrain/Gras/Schilf, Wind
eingefroren, Kamera rueckt pro Frame eine FESTE Strecke (Meter, nicht m/s) radial
auf den Baum zu. Pro Frame eine Zeile mit allem, was die Darstellung bestimmt
(Entfernung, Stufe, R1/R2, alle vier Schichtgewichte, Geometrie-Mengen,
Existenz-Entscheidungen samt Schnittweite, beide Saetze, fade, Upload), dazu
`LT-STUFE`, `LT-GEOMETRIE`, `LT-SUMME`, `LT-ERGBNIS`.

Messung 140 m -> 3 m in 0,5 m-Schritten (274 Frames, R1 33,7 m, R2 101,1 m):
Stufenfolge 0 -> 2 -> 3 bei 89 m und 27 m; groesstes Deckungsloch 0,0000;
max |dw/dd| 0,222 (Schranke 0,223); drei Geometrie-Wechsel (139,5 / 126,5 /
52,0 m), jeder bei Gewicht 0,000 und ausserhalb der Blende; 11 Bauten.
Ergebnis OK — die Ein-Baum-Entfernungslogik ist damit als stetig belegt.

**Nebenbefund (erklaert Spruenge, die der Ein-Baum-Test selbst nicht zeigt):**
11 Bauten x ~14 Frames Ueberblendung = 154 von 274 Frames, in denen
`treeFadeNew < 1` ist. In genau diesen Frames schreiben Kronen und Blaetter
**keine Tiefe**, und ein Farbmaske-aus-Nachpass schreibt Tiefe nur fuer Fragmente
mit Deckung >= 0,5 — die Tiefen-Silhouette des Baums kippt also pro Fragment bei
0,5. Im Vollbild (Terrain/Gras hinter dem Baum) ist das ein Umschalten der
Verdeckung, also ein Sprung. Der Satz-Wechsel selbst ist laut Protokoll harmlos:
alter und neuer Satz waren in allen Ueberblend-Frames identisch (45/45, 840/840,
0/0).

Der Kern kennt jetzt den Betriebsmodus `bb` (liegen Ganzbaum-Kacheln im Puffer).
Ohne dieses Feld rechnete der Test „Billboard 1,000 / Karten 0,000" fuer einen
Baum, der im Bild stand — Modell und Shader waren nicht mehr dasselbe.
`make test-lod` prueft beide Modi: 14 795 Pruefungen, 0 Fehler.

## Pumpen-Periode gefunden: Schnitt in einem Bereich mit Gewicht 1 (2026-10-08)

Nutzer-Logs (lod.log, lod2.log, Host/NVIDIA, ein Baum, R1 20,0 m, R2 57,9 m,
Schrittweite 0,05 m, 2740 Frames) statt VM-Screenshots. Auswertung:

* Rebuilds alle **17,0 m** (Ausloeser bei 122,8 / 105,9 / 88,9 / 71,9 / 54,9 /
  37,9 / 20,9 / 3,9 m).
* In 395 Frames laeuft eine Satz-Ueberblendung; in **92** davon sind alter und
  neuer Satz UNTERSCHIEDlich, z. B. bei d = 71,90 m: neu 10 164 Geruest-Vertices,
  alt 45 (nur Stamm). Das Geruest-Gewicht ist dort durchgehend 1,000 — es wird
  also eine Schicht mit Gewicht 1 ueber ~14 Frames ein- bzw. ausgeblendet.
  Genau das ist das periodische Pumpen, Periode = Rebuild-Abstand.

Ursache: `treeLodBranchCut()`/`treeLodMeshCut()` schnitten bei R2*1,15 + Nachlauf
(hier 86,6 m). Ohne Ganzbaum-Kacheln im Puffer blendet das Geruest aber NIE aus
(Gewicht 1 bis zur Bau-Grenze) — der Schnitt lag in einem Bereich mit Gewicht 1.
Ein Schnitt darf per Definition nie in einem Bereich mit Gewicht > 0 liegen.
Korrektur: ohne Kacheln gar nicht schneiden (`LOD_NO_CUT`). Neuer Wirtstest-Zweig
prueft genau das ueber die ganze Bau-Reichweite: 22 015 Pruefungen, 0 Fehler.

Nebenbei: `DEMO_ONE`/`DEMO_LODTEST` setzten `envNoTerrain`, das weiter unten bei
`DEMO_NOTERRAIN` wieder auf 0 gelesen wurde — der Ein-Baum-Test renderte die
ganze Zeit Terrain (Nutzerbefund). Jetzt per `setenv`.

Test beschleunigt: kleine Aufloesung reicht, die VM-Zeit steckte in der Fuellrate
(640x360-Raymarch). `DEMO_LODTEST="140 3 0.02" ./bin/64kdemo 32 24` = 6 850 Frames
in unter 4 Minuten.

## Pumpen gemessen: Satz-Ueberblendung ist nicht deckungserhaltend (2026-10-08)

Modellfreie Messung im Ein-Baum-Test: `LT-DECKUNG` zaehlt pro Frame helle Pixel
der ganzen Bildflaeche (Terrain aus, also = sichtbare Baumdeckung). Read erst
nach dem MSAA-Resolve (vorher kam aus dem Multisample-FBO Muell: konstant 6 HUD-
Pixel). Kamera zielt im Test pro Frame auf die Baummitte, sonst wandert der Baum
aus dem Bild.

Ergebnis mit Satz-Ueberblendung (treeFadeDur 0.9 s): in JEDEM Ueberblend-Fenster
faellt die Deckung unter den Wert der Frames davor und danach —
115,6->114,5 m: 133 -> 122 -> 135 px, 103,5 m: 161 -> 151 -> 167 px,
91,4 m: 204 -> 193 -> 209 px. Periode = Rebuild-Abstand. Das ist das Pumpen.

Grund: zwei identische halbtransparente Kopien mit komplementaerem Alpha sind
nicht deckungserhaltend. Mischt man eine Schicht mit Deckung a bei Fade f zweimal,
steht danach a - a^2 f (1-f); bei f = 0.5 fehlen bis zu 25 %. Die Regel "w und
1-w" gilt nur fuer disjunkte Schichten, nicht fuer dieselbe Geometrie doppelt.

Konsequenz: Satz-Ueberblendung aus (treeFadeDur 0.0, DEMO_TREEFADE holt sie
zurueck). Sie ist ueberfluessig, seit Geometrie-Unterschiede zwischen Saetzen
nur noch bei Gewicht 0 auftreten (Wirtstest-Zweig + LT-PUMP-Pruefung im Engine-
Test: 0 Treffer).

Vorher/Nachher, groesste Deckungs-Dellen im Bereich 60-140 m:
  mit Ueberblendung  -4,31 % / -3,85 % / -3,75 % / -3,45 % / -3,18 %
  ohne Ueberblendung -1,27 % / -1,20 % / -1,18 % / -1,12 % / -1,12 %
Rest ~1-2 Pixel bei ~150 px Deckung = Messraster.

Test schneller: `DEMO_LODTEST="140 3 0.1" ./bin/64kdemo 320 180` (1370 Frames,
~2,5 min). Die Laufzeit steckt in der Fuellrate, nicht in der Logik.

## Vollstaendiges Geruest bis zur Bau-Grenze: Kosten und BSS-Grenze (2026-10-08)

Nach dem Wegfall des Geruest-Schnitts (Gewicht 1 bis zur Bau-Grenze) braucht die
Standardszene **9 863 016 Geruest-Vertices**. Bei 9,5 Mio riss das Cap (399
Treffer, 26 Baeume uebersprungen). Hochsetzen auf 24 Mio liess den Linker
abarbeiten: `relocation truncated to fit: R_X86_64_PC32 against symbol ... in
.bss` — die Puffer lagen als statische Arrays in BSS, und zusammen mit Gras/
Schilf/Terrain wird die 2-GB-Grenze PC-relativer Adressierung ueberschritten.

Loesung: tvA/tvB, cvA/cvB, lvA/lvB auf `calloc` umgestellt (tree_bufs_init()).
Budget ist damit eine Speicherfrage, keine Link-Grenze. TREE_VERT_CAP 9,5 ->
12 Mio. Gemessen: 0 Cap-Treffer, 0 uebersprungene Baeume (Standard und Stress
LODRSCALE 2.0), RSS 2,40 GB (CPU-Doppelpuffer + VAOs).

Offen/abzuwaegen: 2,4 GB und ~9,9 Mio Vertices pro Frame sind der Preis fuer
"jeder Baum hat bis zur Bau-Grenze volles Geruest". Alternative, die die
Invariante einhaelt: Baum-Gewicht am Bau-Rand auf 0 ausblenden (dann ist ein
Schnitt dort erlaubt) und/oder den Bau-Radius verkleinern. crown.vert blendet
Karten bei 190-250 m aus, die Bau-Grenze liegt aber bei 180 m — der Fade wirkt
also nie, und das Geruest hat am Bau-Rand gar keinen Fade (Baum verschwindet
hart). Das haengt mit dem Cache-Thema zusammen.

## Holz-LOD nach Bildgroesse, Aufloesung ohne Transparenz (2026-10-08)

Kriterium statt geratener Baelen: ein Ast mit Radius r deckt in der Dicke 2r
px = 2*r*focalPx/d Pixel ab (focalPx = 1,7 * Bildhoehe, weil build_mvp mit
fy = 3.4 NDC rechnet). Gewicht = smoothstep(0,6 px, 1,6 px, px) — dasselbe in
tree_lod.c (treeLodWBranch) und lod.glsl (lodBranchW), Konstanten im Wirtstest
verglichen. Bandbreite ergibt sich von selbst (r = 1 cm bei 1080p: 37..98 m).

Aufgeloest wird OHNE Blende: jeder Abschnitt bekommt einen stabilen Schwellwert
aus dem Ast-Seed (derselbe Wert auf allen seinen Vertices), und im Shader gilt
`if(diss > w) -> Dreieck weg`. Fragmente bleiben opak, Depth-Write an. Damit gibt
es im Holz keine Transparenz, keine Sortierfrage, keine Z-Artefakte und kein
a - a^2 f (1-f)-Problem. Die Gesamtabdeckung sinkt sanft, weil die duennsten
Abschnitte zuerst fallen (der Radius nimmt entlang des Astes ab -> Spitze vor
Basis).

Existenz und Sichtbarkeit fallen zusammen: treeLodRadCut(rad) = 2*rad*focalPx/
LOD_PX_LO ist genau die Entfernung, ab der das Gewicht 0 ist; der Build schneidet
einen Ast erst dahinter (plus Build-Nachlauf und Baumausdehnung). Ein Rebuild oder
Satz-Wechsel kann damit nichts Sichtbares mehr aendern.

Gemessen (Standard-Szene, 648 Baeume):
  Geruest 9 863 016 -> 1 543 533 Vertices (-84 %), 0 Cap-Treffer, 0 uebersprungen
  Deckung ein Baum (Bildmessung): 140 m 87 -> 80 px, 60 m 468 -> 460,
  30 m 1588 -> 1582, 10 m 2756 -> 2756, 3 m 18425 -> 18425 (Nahe unveraendert)
Wirtstest 30 080 Pruefungen, 0 Fehler (Monotonie, Schnitt genau bei Gewicht 0,
ohne Brennweite kein Schnitt).

Neue harte Zahl im Ein-Baum-Test: `LT-SUMME groesste Deckungs-Delle` (Bild,
lineare Nachbarschaft). Aktuell -5,26 % bei 30,8 m. Ursache ist NICHT das Holz
(unchanged gegen Vorversion), sondern die Uebergabe Einzelblatt -> Karte bei
R1 = 31 m: 12 600 Einzelblaetter gegen 840 Karten, die Karten decken weniger.
Das ist der naechste Punkt.

Nachtrag: TREE_VERT_CAP 12 Mio -> 4 Mio (gemessenes Maximum 1 785 495 ueber vier
Kamerapositionen, 124 % Luft). RSS 2,40 GB -> 1,19 GB, 0 Cap-Treffer, 0
uebersprungene Baeume (auch Stress DEMO_LODRSCALE=2.0).

## Drei Nutzerfehler behoben (2026-10-08, 2. Runde)

1. Weisse Vierecke an Astspitzen: die Bildgroessen-Aufloesung entschied PRO
   VERTEX (Radius pro Ring verschieden, a->r / b->r). Ein Dreieck konnte mit
   einem weggeklappten und zwei stehenden Vertices aufreissen. Jetzt tragen alle
   Vertices eines Abschnitts denselben Radius und dieselbe Schwelle (gDissRad),
   und der Ast-Deckel erbt denselben Wert (sonst schwebt er ueber einem
   weggeklappten Abschnitt).
2. Ganzbaum-Kacheln mit "zufaelliger" Orientierung (Taste 1): crown.vert drehte
   das Quad mit cos/sin(aData.y) — seit dem Seitenverhaeltnis in aData.y war das
   eine Drehung um 1,3 RAD (= 74 grad). Kacheln werden jetzt nicht gedreht.
3. Sprung beim Start der Blende ("blaetter werden sprunghaft ein bisschen
   kleiner, der Texeluebergang wandert um ein Texel"): crown.frag/leaf.frag
   schrieben `vFade >= 0.999 ? 1.0 : cov`. Beim ersten Tick der Blende wurde aus
   a = 0,5 schlagartig alpha 0,4995 — die weiche Kartekante wurde instant
   durchsichtig. Alpha ist jetzt durchgehend cov (bei vFade = 1 dasselbe wie
   vorher a, keine Kantenregel wechselt waehrend einer Blende).

Alpha-to-Coverage standardmaessig AN (DEMO_TABC=0 schaltet ab): groesste
Deckungs-Stufe ueber den ganzen Anflug 140->3 m: aus -8,16 % bei 31,1 m,
an -4,04 %. Mit Alpha-Blending + Depth-Write verdeckt die weiche Kante einer
Karte die dahinterliegende; mit A2C belegt sie nur Teile der Samples.

Metrik geaendert: Deckungs-Stufe = Mittelwert der 3 Frames davor gegen den der
3 Frames danach. Der alte Einzelbild-Vergleich (Mitte gegen lineare
Nachbarschaft) meldete -83 % bei 79,3 m, wo die Rohzahlen glatt 251 px zeigen.
Restliche Stufe -4 %: Uebergabe Einzelblatt -> Karte (12 600 Blaetter gegen
840 Karten, Karten decken weniger) — naechster Punkt.

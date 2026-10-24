# Quellen

Worauf dieser Code zurückgeht — getrennt nach **übernommenem Code**, **Konzepten**
und **recherchierten, aber nicht benutzten** Dingen.

## Übernommener Code

**EZ-Tree — Daniel Greenheck** · <https://github.com/dgreenheck/ez-tree> · MIT
(Copyright (c) 2024 Daniel Greenheck), Version 1.1.0.

Der Baumgenerator ist eine Übersetzung dieser JavaScript-Bibliothek nach C.
Standort bei uns: `src/trees_cpu.c`, Funktion `ez_grow` und die Preset-Werte.
Umschaltbar zur früheren eigenen Generation mit `DEMO_TREEGEN=alt|ez`
(Standard: `ez`). Übernommen sind unter anderem:

* Abschnitts-Ast mit Segmentlängen, Gnarliness (`1/sqrt(Radius)`),
  Wachstums-Kraft `strength/radius` um `cross(Astachse, up)`, Twist,
* geschichtete Kinder mit vertauschten Radial-Slots, Radius immer aus dem
  Elternast, `taper` und `closeTip` pro Level,
* die Presets `oak`, `ash`, `aspen`, `pine` aus `src/lib/presets/*_medium.json`,
* die LOD-Semantik (Abschnitts-Dichte und Blattgröße nach Stufe, Hysterese).

Einzelne Übernahmen sind in `STATUS.md` zeilenweise belegt, zum Beispiel der
Längen-Abfall bei immergrünen Bäumen (`tree.js:534-538`) und der
Spitzen-Fortsetzungsast (`tree.js:424`).

**Header in `third_party/include/`** — OpenGL-/GLX-Header der Khronos Group
(`GL/*.h`, `SPDX-License-Identifier: MIT`) und X11-Header (MIT-artige X-Lizenz).
Unverändert, nur zum Übersetzen; es wird nichts dagegen gelinkt (Laufzeit-Bindung
über `dlopen`, siehe `src/main.c` am Beispiel von `libGL.so.1`).

## Konzepte (kein Code übernommen)

* **GPU Gems 3, Kapitel 4 — SpeedTree**
  <https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch04.html>:
  Alpha-Test-Cutouts statt Alpha-Blending für Laub, *fizzle* als
  Reihenfolgen-unabhängiger LOD-Übergang, Alpha-to-Coverage nur für
  Silhouettenkanten.
* **GPU Gems 3, Kapitel 16 — Crysis**: Leaf-Cards, Distant-Sprite-Impostoren,
  Wind-Animation der Halme/Blätter.
* **GPU Gems 3, Kapitel 21 — True Impostors**: Orientierung ferner
  Impostor-Flächen an festen Blickrichtungen statt am Kameravektor.

Diese Kapitel sind frei lesbar; übernommen sind daraus Verfahren und
Entscheidungen, nicht Zeilen.

## Recherchiert, nicht benutzt

* **Blender „Sapling Tree Gen“** (Add-on v0.3.7) — Notiz in `STATUS.md`,
  Abschnitt „Recherche“. Kein Code übernommen.

## Werkzeug und Entstehung

Der gesamte Stand ist mit einem lokalen Sprachmodell über den Coding-Agenten
**pi** (<https://github.com/earendil-works/pi>) entstanden. Die Dateien
`protocol*.txt` und `protocoll.txt` sind Exporte der pi-Erweiterung `protokoll`
(`~/.pi/agent/extensions/protokoll`): Sie schreibt zu jedem Zug ein Protokoll
(Ergebnis, Entscheidungen, Dateien, offene Punkte) und hält damit die
Entstehung des Codes dokumentierbar. `STATUS.md` ist das dazugehörige
Arbeitsprotokoll des Projekts, `AGENTS.md` sind die Regeln, nach denen der Agent
hier arbeitet.

## Lizenz: warum MIT passt

* **EZ-Tree** ist MIT. Eine MIT-Lizenz ist mit MIT beliebig kombinierbar, wenn
  der Urhebervermerk mitgegeben wird — das tut `LICENSE` (upstream-Copyright +
  Verweis auf diese Datei).
* **`third_party/include/`** (Khronos: `SPDX-License-Identifier: MIT`; X11:
  MIT-artige X-Lizenz) verlangt ebenfalls nur, dass die Vermerke in den Dateien
  bleiben. Die Header sind unverändert, also erfüllt.
* **Copyleft ist nicht im Spiel:** es wird kein Code unter GPL oder ähnlichem
  benutzt. Wichtig falls sich das mal ändert: **Blender „Sapling Tree Gen“ steht
  unter GPL** — daraus ist bisher nichts übernommen (nur recherchiert). Würden
  wir Code daraus übersetzen, wäre eine reine MIT-Lizenzierung des Ganzen nicht
  mehr haltbar.
* **Zitierte Passagen** (GPU Gems, Vorträge) bleiben das Copyright ihrer
  Autoren; sie sind hier nur bezeichnet, nicht abgedruckt.

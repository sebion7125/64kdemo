# 64kDemo — prozedural erzeugte Waldlandschaft in C + OpenGL

Deutsch · [English](README.en.md)

Ein Geländestück mit Bäumen, Gras und Schilf, komplett prozedural zur Laufzeit
erzeugt: kein Modell, keine Textur-Datei, keine Asset-Pipeline. Ursprünglich als
64-k-Demo angefangen (daher der Name); das Größenziel ist heute keine Vorgabe
mehr, interessant ist die prozedurale Erzeugung.

Der Stand ist **Arbeitsstand, keine Veröffentlichung im engeren Sinn**: Es wird
weiter an Sichtweiten, Übergängen und Performance gearbeitet.

Der Code ist mit einem lokalen Sprachmodell geschrieben. Welches Modell, in
zwei Phasen über wie viele Sitzungen, und warum die `protocol*.txt` im Repo
liegen: [ENTSTEHUNG.md](ENTSTEHUNG.md) (englisch: [ENTSTEHUNG.en.md](ENTSTEHUNG.en.md)).

## Bilder

![Waldrand: Bäume mit Blättern, Gras und Schilf am Teich](docs/bilder/waldrand.jpg)

Aufnahme aus Augenhöhe, 2026-10-24. Vorne die Bäume mit einzeln gezeichneten
Blättern, in der Bildmitte und dahinter dieselbe Gegend in Büschel-Quadraten und
Kronenhüllen — umgeschaltet wird nach der Größe, die ein Blatt auf dem
Bildschirm hätte, nicht nach festen Metern. HUD: 3400 Bäume, 869 573
Kronen-Quadrate, 2 603 439 Grashalme, Fenster 3840x2030 bei halber interner
Auflösung, 4x MSAA, 31 fps auf dem Wirtsrechner.

![Kamerafahrt vom Teich in die Wiese zwischen den Bäumen](docs/bilder/fahrt.gif)

Kamerafahrt über dieselbe Welt: vom Wasser zwischen die Bäume. 80 Bilder in
8 Sekunden, aufgenommen aus einem 1280x720-Fenster; die Bildrate im HUD bleibt
dabei zwischen 50 und 61 fps.

![Fernsicht: Gewässer, Schilf und Wald bis in den Dunst](docs/bilder/fernsicht.jpg)

Aus der Höhe über dieselbe Welt: flaches Gewässer mit Schilfzonen, Wald bis an
die Dunstgrenze — jenseits davon wird nichts mehr gebaut. HUD: 3400 Bäume,
869 983 Kronen-Quadrate, 36 fps.

## Was drin ist

* **Terrain**: Höhenfeld prozedural, im Farbpass geraymarcht (Höhensprünge statt
  Mesh), Dunst aus der gleichen Funktion wie die Objekte.
* **Bäume**: Generator ist eine C-Portierung von
  [EZ-Tree](https://github.com/dgreenheck/ez-tree) (MIT, siehe `QUELLEN.md`),
  Presets Eiche/Esche/Zitterpappel/Kiefer. Knospen, Achsen, Verzweigung,
  Radien und Blattbüschel entstehen aus einem Seed — derselbe Seed, derselbe Baum.
* **Drei Laub-Darstellungen** pro Baum, abhängig von der Projektgröße auf dem
  Bildschirm: Einzelblätter → Büschel-Karten → Kronenhülle (Zylinderraster aus
  den Büschelpositionen, Lücken bleiben durchsichtig). Übergänge laufen
  baumweise über einen Zwei-Puffer-Composite, nicht über Alpha-Blending.
* **Gras und Schilf**: instanziierte Halme bzw. eigene Pass-Implementierung mit
  Dichtefeld und Uferzonen-Erkennung.
* **Wind**: ein Boenfeld, das von Ästen, Blättern, Karten, Hülle und Gras
  identisch benutzt wird.
* **Kein Link gegen OpenGL/X11**: alles zur Laufzeit über `dlopen`, dadurch
  übersetzt und läuft auch ohne develop-Pakete.

## Bauen und Starten

Voraussetzungen: `gcc` (c11), zur Laufzeit `libX11.so.6` und `libGL.so.1` mit
einem OpenGL-4.3-Kontext (NVIDIA/Mesa genügen). Die nötigen Header liegen in
`third_party/include`.

```sh
make                      # baut bin/64kdemo und erzeugt src/shaders_gen.c
./bin/64kdemo 1920 1080   # Fenstergröße; ohne Argumente 1280x720
```

`make` kopiert die Binärdatei zusätzlich nach `/media/sf_Quarantine/64kDemo`
(Virtueller-Maschinen-Freigabe). Wo das nicht existiert oder schreibt, gibt es
nur eine Warnung; der Build läuft normal weiter.

### Tasten

```
Maus=Schauen  WASD=Fliegen  Space/X=Hoch/Runter  Shift=Turbo
O=Orbit  R=Reset  1/2/3=Skala  T=Wire  P=Screenshot  M=MSAA  H=HUD  Q/Esc=Ende
4=nur Blätter  5=nur Büschel-Karten  8=nur Kronenhülle  7=alle Laub-Stufen
0=Composite (LOD-Übergang) aus/an
```

### Umgebungs-Variablen

Es gibt ~90 Mess- und Diagnose-Schalter (`grep -o 'getenv("DEMO_[A-Z]*")' src/main.c | sort -u`).
Die nützlichen:

| Variable | Wirkung |
|---|---|
| `DEMO_CAM="x z pitch yaw"` | Kamera setzen (Höhe folgt dem Terrain) |
| `DEMO_WALK="x 0"` | Kamera Fahrt aufnehmen lassen (Bewegungs-Test) |
| `DEMO_TREEGEN=ez\|alt` | Baumgenerator: EZ-Tree-Port oder die alte eigene Version |
| `DEMO_EZPRESET=0..3`, `DEMO_EZSPREAD=` | Baumtyp und Wuchsform (3 = Nadelholz) |
| `DEMO_BUILDTIME=1` | Bau- und Upload-Zeiten messen |
| `DEMO_LODTEST="d1 d2 schritt"` | ein Baum, sonst nichts, Kamera mit fester Stufe darauf zu |
| `DEMO_LAYERS="leaf\|card\|hull"` | einzelne Laub-Stufe anzeigen |
| `DEMO_COMP=0/1` | Zwei-Puffer-Übergang aus/an |
| `DEMO_EXIT=1 [DEMO_EXITFRAME=n]` | nach Build+Upload beenden (Messläufe) |
| `DEMO_SHOTFRAME=n` | Bild nach n Frames schreiben |

## Tests

```sh
make test-lod     # LOD-Kern ohne GL: Bänder, Stetigkeit, Existenz-Ränder  (~140 000 Prüfungen)
make test-tree    # Baum-Bau ohne GL: gleiche Geometrie bei jeder Build-Entfernung
tools/selftest.sh # fährt mehrere Konfigurationen: keine GL-Fehler, keine Cap-Treffer,
                  # keine übersprungenen Bäume, zwei gleiche Läufe bitgleich
```

Der Selbsttest ist auf Reproduzierbarkeit gebaut, nicht auf Bildbewertung:
Er vergleicht zwei Läufe derselben Konfiguration Pixel für Pixel. Wie es aussieht,
beurteilt der Mensch.

## Repo

```
src/main.c        Fenster, GLX, Zeichenreihenfolge, alle Pässe, HUD, Screenshots
src/shaders/      GLSL (wird von tools/gen_shaders.sh zu src/shaders_gen.c)
src/trees_cpu.c   Baumgenerator (EZ-Tree-Port), Büschel, Kronenhülle
src/tree_lod.c    Schaltweite aus Projektgröße, Bänder, Gewichte — rein, ohne GL
src/grass_cpu.c   Halme, Dichtefeld, Ferntextur
src/reed_cpu.c    Schilf (Uferzonen)
src/terrain_cpu.c Höhenfeld, Dunst
tools/            Shader-Erzeugung, GL-freie Wirtstests, selftest.sh
STATUS.md         Arbeitsprotokoll: jede Runde mit Messung und Fehlern
ENTSTEHUNG.md     Hintergrund: welche Modelle, zwei Phasen, zehn Sitzungen und eine
protocol*.txt     Protokolle der Agent-Sitzungen (Entstehung, siehe QUELLEN.md)
```

## Stand (Kurzfassung)

Die Waldkante reicht bis zur Nebelgrenze (~450 m), Bäume werden in einem
Worker-Thread erzeugt und in Scheiben hochgeladen. Die Übergänge zwischen den
Laub-Stufen funktionieren über den baumweisen Composite; an der Kronenhülle und
an der gleichmäßigen Bau-Kante (kein Verschwinden ganzer Ringe bei Kamerafahrt)
wird noch gearbeitet. Nadeln statt Laubblättern an Nadelhölzern, Blattstile und
das Windmodell (Biegen statt Verschieben an Verzweigungen) sind vorgemerkt.

## Lizenz und Quellen

MIT, siehe `LICENSE`. Worauf der Code zurückgeht, steht in `QUELLEN.md` —
der Baumgenerator ist eine Portierung von EZ-Tree (Daniel Greenheck, MIT),
deshalb ist auch dieses Projekt unter MIT gestellt.

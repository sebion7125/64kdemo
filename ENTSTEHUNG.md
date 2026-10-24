# Wie das entstanden ist

Angefangen hat es als Modelltest. Ich wollte sehen, wie weit ein lokales Modell auf meinen zwei RTX 3090 bei einem größeren Projekt kommt, und eine prozedurale Grafikdemo schien mir ein würdiges Thema dafür. Eine der beiden Karten ist inzwischen ausgefallen; Strata läuft aber auch auf einer allein ausreichend schnell. Gearbeitet habe ich mit dem Coding-Agenten [Pi](https://pi.dev), zuerst mit Huihui-Qwen3.8-27B-abliterated (EXL3, 4,0 bpw) über TabbyAPI, ab dem 6. Oktober mit Qwen 3.8 Flash Next (IQ3_S) über die Inferenz-Engine [Strata](https://github.com/Niko1221/Strata).

**Phase 1, 3. bis 7. Oktober: zehn Sitzungen.** Terrain, Gras mit Ein- und Ausblenden, Schrift und HUD, erste Bäume. Mit Pis eigenem Compacting kam jede Sitzung auf zwei bis vier Compactings, jeweils bei rund 242.000 Token. Das kostete jedes Mal Wartezeit, und mit jedem Durchgang ging mehr verloren. Irgendwann drehte sich das Modell nur noch im Kreis, der Kontext war vergiftet. Dann ließ ich es den Stand in `STATUS.md` schreiben und fing eine neue Sitzung an. Das ist auch der Grund, warum `STATUS.md` so lang ist.

Das müsste doch besser gehen. Daraus wurde [pi-protokoll](https://github.com/sebion7125/pi-protokoll): Das Modell schreibt am Ende jedes Zugs ein kurzes Protokoll, solange es den Zug noch vollständig vor sich hat. Beim Compacting setzt Code daraus eine gestufte Zusammenfassung zusammen, statt das Modell den ganzen Verlauf neu zusammenfassen zu lassen, und jeder frühere Zug bleibt über Werkzeuge nachschlagbar.

**Phase 2, ab dem 7. Oktober abends: eine einzige Sitzung.** Zur selben Zeit begann der Port von [EZ-Tree](https://github.com/dgreenheck/ez-tree), also das ganze Baumsystem mit Laub-Stufen, Übergängen und Wind. Diese Sitzung lief mit der Erweiterung bis zur Veröffentlichung des Repos durch: 179 Nachrichten, 23 Compactings, jedes in rund 30 Sekunden statt Minuten. Eine neue Sitzung war nicht mehr nötig.

Die Dateien `protocol*.txt` sind Exporte dieser Protokolle aus Phase 2.

---

*englische Fassung: [ENTSTEHUNG.en.md](ENTSTEHUNG.en.md)*

# 64kDemo — Projekt-Regeln für den Agenten

## Git
- Vor jeder Änderungsrunde: `git status` muss sauber sein, sonst nachfragen.
- Nach jeder funktionierenden Änderung: committen mit kurzer Nachricht (was und warum).
- Nie `git reset --hard`, `git push --force` oder `git clean` ohne ausdrückliche Anweisung.
- `src/shaders_gen.c` ist generiert (Makefile ← `src/shaders/*.glsl` via `tools/gen_shaders.sh`)
  und im .gitignore — nie von Hand editieren, immer die .glsl-Datei ändern.
- `STATUS.md` bei Änderungen aktuell halten.
## Messlaeufe und Tests
- Jede Messung, jeder Engine-Lauf und jeder Test erhaelt einen expliziten `timeout`
  (kurz: 60-120 s fuer Einzelbilder, 300 s fuer den Selbsttest). Nie ohne timeout starten.
- Laeufe ueber ~30 s immer im Hintergrund starten (`... >/tmp/x.txt 2&1 &`) und
  Ergebnis spaeter lesen. Der Build wird sofort ausgeliefert, waehrend der Lauf laeuft.
- Eigene Analyse-Skripte (z.B. Bildvergleiche) muessen nach oben begrenzt sein
  (feste Schleifengrenze, Datei-Groesse pruefen) — ein while-Suchen ohne Fund
  darf nie endlos laufen.
- `pkill -f <muster>` ist verboten, wenn das Muster in der eigenen Befehlszeile
  steht (signiert dem eigenen Shell-Prozess SIGTERM); stattdessen `pkill -x <programm>`.

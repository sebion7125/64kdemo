# 64kDemo — Projekt-Regeln für den Agenten

## Git
- Vor jeder Änderungsrunde: `git status` muss sauber sein, sonst nachfragen.
- Nach jeder funktionierenden Änderung: committen mit kurzer Nachricht (was und warum).
- Nie `git reset --hard`, `git push --force` oder `git clean` ohne ausdrückliche Anweisung.
- `src/shaders_gen.c` ist generiert (Makefile ← `src/shaders/*.glsl` via `tools/gen_shaders.sh`)
  und im .gitignore — nie von Hand editieren, immer die .glsl-Datei ändern.
- `STATUS.md` bei Änderungen aktuell halten.
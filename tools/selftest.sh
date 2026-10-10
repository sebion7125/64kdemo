#!/bin/sh
# Selbsttest fuer 64kDemo — faehrt eine feste Menge an Konfigurationen und prueft
# harte Kriterien (kein GL-Fehler, keine uebersprungenen Baeume, Caps nicht
# gerissen, Bild deterministisch, Build laeuft durch).
#
# Aufruf:  tools/selftest.sh [WIDTH HOEHE]      (Default 640 360)
#
# Warum in der VM mit Software-GL (llvmpipe): die Bildvergleiche sind nur
# reproduzierbar, wenn derselbe Stand zweimal laeuft — nicht, um die Optik zu
# beurteilen. Dafuer ist der Host mit GPU da.

set -u
W=${1:-640}; H=${2:-360}
BIN=${BIN:-./bin/64kdemo}
OUT=${OUT:-/tmp/64kdemo_selftest}
mkdir -p "$OUT"
fails=0
log(){ printf '%s\n' "$*"; }
fail(){ printf 'FEHLER: %s\n' "$*"; fails=$((fails+1)); }

run(){   # run <name> <timeout_s> <env...>
  name=$1; to=$2; shift 2
  rm -f /tmp/64kdemo_shot.ppm
  env DISPLAY=${DISPLAY:-:0} DEMO_FREEZE=1 "$@" \
      timeout "$to" "$BIN" "$W" "$H" > "$OUT/$name.log" 2>&1
  pkill -9 -x 64kdemo 2>/dev/null
  sleep 0.3
  [ -f /tmp/64kdemo_shot.ppm ] && cp /tmp/64kdemo_shot.ppm "$OUT/$name.ppm"
}

check_tree_counts(){   # check_tree_counts <name>
  name=$1
  caps=$(grep -h "Caps:" "$OUT/$name.log" | tail -1)
  skip=$(grep -h "uebersprungen" "$OUT/$name.log" | tail -1 | sed 's/[^0-9]*\([0-9]*\).*/\1/')
  [ -n "$caps" ] || { fail "$name: keine Caps-Zeile (Build gelaufen?)"; return; }
  case "$caps" in
    *"TREffer: Geruest 0 Karten 0 Blaetter 0"*) : ;;
    *) fail "$name: Puffer-Cap gerissen -> $caps" ;;
  esac
  [ "${skip:-x}" = "0" ] || fail "$name: $skip Baeume wegen Budget uebersprungen"
}

check_no_glerror(){
  name=$1
  if grep -qiE "GL-Fehler|glGetError.*0x[1-9a-f]" "$OUT/$name.log"; then
    fail "$name: GL-Fehler im Log"
  fi
  # "Shaderfehler (tmesh):" ist die Ausgabe von make_program in src/main.c —
  # ohne diesen Treffer lief ein Lauf mit kaputtem Shader durch (2026-10-10:
  # uFarCut in tree.vert benutzt, aber nicht deklariert; auf dem Host brach die
  # Engine damit sofort ab, in der VM lief der alte Binary).
  if grep -q "FAILED\|failed to compile\|Shader-Fehler\|Shaderfehler\|Shader-Binärfehler" "$OUT/$name.log"; then
    fail "$name: Shader-Kompilierungs-/Linkfehler"
  fi
}

log "== 64kDemo Selbsttest  (bin=$BIN, ${W}x$H, Software-GL in der VM) =="

# 1) Standardszene, feste Kamera
# DEMO_SHOTFRAME statt DEMO_SHOT: die Determinismus-Paarung vergleicht zwei
# Laeufe Pixel fuer Pixel. DEMO_SHOT=<s> fuehrt den Screenshot nach REALZEIT,
# bei unterschiediger Rechnerlast also bei unterschiedlicher FRAME-NUMMER (gemessen:
# standard mit 0,22 fps -> Schuss bei Frame 1, repeat mit 60 fps -> Frame 380;
# 225 055 unterschiedliche Pixel, gruen 16 979 vs 7 778). Der Vergleich selbst
# bleibt unveraendert streng.
# DEMO_EXIT=1: der Schuss faellt, wenn Build + Upload + Kacheln fertig sind,
# nicht bei einer festen Frame-Nummer. Mit der Baukante von 450 m braucht der
# Upload in der VM mehrere Frames (2026-10-10: DEMO_SHOTFRAME=8 erwischte den
# halbhohen Upload -> 194 364 unterschiedliche Pixel zwischen zwei Laeufen).
run standard 60 "DEMO_CAM=5 16 -0.15 0.7" "DEMO_EXIT=1"
check_tree_counts standard; check_no_glerror standard

# 2) Zweiter Lauf derselben Konfiguration: Bild muss bitgleich sein
#    (Laufzeit-abhaengige Effekte sind mit DEMO_FREEZE=1 abgeschaltet)
run repeat 60 "DEMO_CAM=5 16 -0.15 0.7" "DEMO_EXIT=1"
if [ -f "$OUT/standard.ppm" ] && [ -f "$OUT/repeat.ppm" ]; then
  # Der HUD-Text (FPS-Zeile oben) zeigt die Laufzeit, ist also per Definition
  # nicht deterministisch. Gemessen: die einzigen Unterschiede beider Laeufe
  # lagen in y 3..38, x 633..639 (123 Pixel, max Delta 9) — genau dort.
  # Verglichen wird deshalb die Szene unterhalb des HUD; der HUD-Streifen wird
  # getrennt gemeldet, nicht ignoriert.
  if python3 - "$OUT/standard.ppm" "$OUT/repeat.ppm" <<'PYEOF'
import sys
def ppm(p):
    d=open(p,'rb').read(); parts=d.split(b'\n',3)
    w,h = parts[1].split()
    return int(w), int(h), parts[3]
w,h,a=ppm(sys.argv[1]); _,_,b=ppm(sys.argv[2])
HUD=56   # HUD-Kasten: drei Textzeilen bei 640x360 reichen bis y=50 (war 48)
def diff(buf,off):
    c=0; mx=0
    for i in range(off*3, len(buf), 3):
        d=max(abs(a[i+k]-b[i+k]) for k in range(3))
        if d: c+=1; mx=max(mx,d)
    return c,mx
scene,sceneMax = diff(a, HUD*w)
hud,hudMax = diff(a, 0)
print("  Szene (y>=%d): %d unterschiedliche Pixel, max Delta %d"%(HUD,scene,sceneMax))
print("  HUD-Streifen (y<%d): %d Pixel, max Delta %d (FPS-Text, zeitabhaengig)"%(HUD,hud,hudMax))
sys.exit(0 if scene==0 else 1)
PYEOF
  then
    log "OK   Bild deterministisch (Szene bitgleich, HUD ausgenommen)"
  else
    fail "Bild nicht deterministisch: standard.ppm != repeat.ppm"
  fi
else
  fail "Screenshots fehlen (standard/repeat)"
fi

# 3) Ein Baum, jede Detailstufe — Build muss durchlaufen, Caps frei bleiben
for tier in 3 2 0; do
  run "tier$tier" 15 "DEMO_ONETREE=1" "DEMO_TREEDIST=24" "DEMO_TIER=$tier" \
      "DEMO_NOTERRAIN=1" "DEMO_SHOT=6"
  check_tree_counts "tier$tier"; check_no_glerror "tier$tier"
done

# 4) Kameraweg: Baut und uebergblendet dauerhaft, ohne dass Baeume fehlen
run walk 25 "DEMO_CAM=5 16 -0.15 0.7" "DEMO_WALK=13 0" "DEMO_TREEDEBUG=1"
check_tree_counts walk; check_no_glerror walk
sw=$(grep -c "Satz-Wechsel" "$OUT/walk.log")
[ "$sw" -ge 1 ] || fail "walk: kein Satz-Wechsel (Build/Upload laeuft nicht)"
log "OK   walk: $sw Satz-Wechsel, Upload-Slices: $(grep -c 'Baum-Upload' "$OUT/walk.log")"

# 5) Kleinster Build-Radius (schneller Build) und grösster (alles in Stufe 3)
run small 20 "DEMO_CAM=5 16 -0.15 0.7" "DEMO_TREEBUILDRANGE=5" "DEMO_SHOT=4"
check_tree_counts small; check_no_glerror small
run big 25 "DEMO_CAM=5 16 -0.15 0.7" "DEMO_LODRSCALE=2.0" "DEMO_SHOT=4"
check_tree_counts big; check_no_glerror big

# 6) Schilf-Pass: an einem Ufer muss der eigene Build Pflanzen liefern
run reed 25 "DEMO_CAM=204 314 -0.10 0.5" "DEMO_ARTEN=2" "DEMO_SHOT=6"
check_no_glerror reed
np=$(grep -h "Schilf-Build:" "$OUT/reed.log" | head -1 | sed 's/[^0-9]*\([0-9]*\).*/\1/')
[ "${np:-0}" -gt 0 ] || fail "reed: Schilf-Build liefert 0 Pflanzen"
log "OK   reed: $np Pflanzen, $(grep -h 'Schilf-Build:' "$OUT/reed.log" | tail -1)"

# 7) Referenz ohne Gras (Gras-Sichtweite 0) — Baum-Bild isoliert
run nograss 20 "DEMO_CAM=5 16 -0.15 0.7" "DEMO_GRASSMD=1" "DEMO_SHOT=4"
check_tree_counts nograss

log "== Ergebnis =="
if [ "$fails" -eq 0 ]; then
  log "ALLE PRUEFUNGEN OK — Log unter $OUT"
else
  log "$fails FEHLER — Log unter $OUT"
fi
exit "$fails"

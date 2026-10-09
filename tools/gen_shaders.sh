#!/bin/sh
# Erzeugt src/shaders_gen.c aus den GLSL-Dateien (64k-Style: alles embedded).
# Shader werden minifiziert (tools/minify_glsl.py: Kommentare/Leerzeilen
# raus) und als EINER String-Literal eingebettet (statt Zeile-fuer-Zeile:
# spart ~2 Byte pro Zeile an Anführungszeichen).
# #include "x.glsl" durch den Inhalt von x.glsl ersetzen (rekursiv).
# Damit teilt der LOD-Kern (src/shaders/lod.glsl) alle drei Baum-Shader: drei
# handgeschriebene Baelen in drei Shadern waren unterschiedlich und erzeugten
# die Spruenge beim Bewegen.
expand() {
  python3 -c '
import os, re, sys
def go(p, seen):
    if p in seen: return ""
    seen.add(p)
    d = os.path.dirname(p)
    out = []
    for line in open(p).read().split("\n"):
        m = re.match(r"\s*#include\s+\"([^\"]+)\"", line)
        out.append(go(os.path.join(d, m.group(1)), seen) if m else line)
    return "\n".join(out)
sys.stdout.write(go(sys.argv[1], set()))
' "$1"
}
emit() {
  printf 'const char %s[] =\n' "$1"
  expand "$2" | python3 "$(dirname "$0")/minify_glsl.py" --embed
  printf '\n\n'
}
emit VS_SRC      src/shaders/vertex.glsl
emit FS_SRC      src/shaders/fragment.glsl
emit BLIT_FS_SRC src/shaders/blit.frag
emit HUD_VS_SRC  src/shaders/hud.vert
emit HUD_FS_SRC  src/shaders/hud.frag
emit TREELINES_VS_SRC src/shaders/treelines.vert
emit TREELINES_FS_SRC src/shaders/treelines.frag
emit TREE_VS_SRC  src/shaders/tree.vert
emit TREE_FS_SRC  src/shaders/tree.frag
emit LEAF_VS_SRC  src/shaders/leaf.vert
emit LEAF_FS_SRC  src/shaders/leaf.frag
emit GRASS_VS_SRC src/shaders/grass.vert
emit GRASS_FS_SRC src/shaders/grass.frag
emit REED_VS_SRC src/shaders/reed.vert
emit REED_FS_SRC src/shaders/reed.frag
emit CROWN_VS_SRC src/shaders/crown.vert
emit CROWN_FS_SRC src/shaders/crown.frag
emit DEPTH_FS_SRC src/shaders/depth.frag
emit COMP_VS_SRC  src/shaders/comp.vert
emit COMP_FS_SRC  src/shaders/comp.frag
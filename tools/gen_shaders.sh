#!/bin/sh
# Erzeugt src/shaders_gen.c aus den GLSL-Dateien (64k-Style: alles embedded).
# Shader werden minifiziert (tools/minify_glsl.py: Kommentare/Leerzeilen
# raus) und als EINER String-Literal eingebettet (statt Zeile-fuer-Zeile:
# spart ~2 Byte pro Zeile an Anführungszeichen).
emit() {
  printf 'const char %s[] =\n' "$1"
  python3 "$(dirname "$0")/minify_glsl.py" --embed < "$2"
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
emit CROWN_VS_SRC src/shaders/crown.vert
emit CROWN_FS_SRC src/shaders/crown.frag
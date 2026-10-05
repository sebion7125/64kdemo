#!/bin/sh
# Erzeugt src/shaders_gen.c aus den GLSL-Dateien (64k-Style: alles embedded)
emit() {
  printf 'const char %s[] =\n' "$1"
  awk '{ gsub(/\\/,"\\\\"); gsub(/"/,"\\\""); printf "\"%s\\n\"\n", $0 }' "$2"
  printf ';\n\n'
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
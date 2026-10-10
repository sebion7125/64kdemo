CC      = gcc
# 64k-Budget (< 65536 Byte): -Os, no-pie (kompakteres Layout),
# keine .eh_frame (kein Unwinding/Backtrace benoetigt),
# -z,noseparate-code (kein .plt.sec, enges Packen)
CFLAGS  = -Os -Wall -Wextra -std=c11 -Ithird_party/include -no-pie -fno-asynchronous-unwind-tables
LDFLAGS = -s -no-pie -Wl,-z,noseparate-code
LDLIBS  = -ldl -lm -lpthread
SRC     = src/main.c src/shaders_gen.c src/terrain_cpu.c src/trees_cpu.c src/grass_cpu.c src/reed_cpu.c src/tree_lod.c
HDRS    = $(wildcard src/*.h)   # ohne Header-Deps baut make bei scene.h-Aenderungen nicht
BIN     = bin/64kdemo
SHARE   = /media/sf_Quarantine/64kDemo

all: $(BIN)
	@mkdir -p $(SHARE)
	@cp $(BIN) $(SHARE)/64kdemo
	@echo "Kopiert nach $(SHARE)/64kdemo"

$(BIN): $(SRC) $(HDRS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(LDLIBS)
	@sz=$$(stat -c%s $@); if [ $$sz -gt 65536 ]; then echo "WARNUNG: Binary $$sz Byte > 65536 (64k-Budget)"; else echo "Binary: $$sz Byte (64k-Budget ok)"; fi

src/shaders_gen.c: src/shaders/vertex.glsl src/shaders/fragment.glsl src/shaders/blit.frag src/shaders/hud.vert src/shaders/hud.frag src/shaders/treelines.vert src/shaders/treelines.frag src/shaders/tree.vert src/shaders/tree.frag src/shaders/leaf.vert src/shaders/leaf.frag src/shaders/grass.vert src/shaders/grass.frag src/shaders/crown.vert src/shaders/crown.frag src/shaders/reed.vert src/shaders/reed.frag src/shaders/lod.glsl src/shaders/comp.vert src/shaders/comp.frag tools/gen_shaders.sh tools/minify_glsl.py
	./tools/gen_shaders.sh > $@

# Wirtstest des LOD-Kerns: OHNE GL, ohne Engine, in Millisekunden.
# Prueft Komplementaeritaet der Baelen, Stetigkeit, Existenz-Raender,
# Hysterese und dass GLSL-Seite und C-Kern dieselben Zahlen benutzen.
# Wirtstest des BAUM-BAUS: derselbe Baum bei verschiedenen Build-Entfernungen.
# Prueft, dass die Geometrie nicht von der Build-Entfernung abhaengt und dass es
# keinen Abstand mit nacktem Stamm gibt.
test-tree: tools/test_tree_build.c src/trees_cpu.c src/terrain_cpu.c src/tree_lod.c
	$(CC) -O1 -Wall -Wextra -std=gnu11 -Isrc -o bin/test_tree_build tools/test_tree_build.c src/trees_cpu.c src/terrain_cpu.c src/tree_lod.c -lm -lpthread
	./bin/test_tree_build

test-lod: tools/test_tree_lod.c src/tree_lod.c src/tree_lod.h
	$(CC) -O1 -Wall -Wextra -std=c11 -o bin/test_tree_lod tools/test_tree_lod.c src/tree_lod.c -lm
	./bin/test_tree_lod

clean:
	rm -rf bin src/shaders_gen.c

.PHONY: all clean test-lod test-tree
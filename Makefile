CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -Ithird_party/include
LDLIBS  = -ldl -lm -lpthread
SRC     = src/main.c src/shaders_gen.c src/terrain_cpu.c src/trees_cpu.c src/grass_cpu.c
BIN     = bin/64kdemo
SHARE   = /media/sf_Quarantine/64kDemo

all: $(BIN)
	@mkdir -p $(SHARE)
	@cp $(BIN) $(SHARE)/64kdemo
	@echo "Kopiert nach $(SHARE)/64kdemo"

$(BIN): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

src/shaders_gen.c: src/shaders/vertex.glsl src/shaders/fragment.glsl src/shaders/blit.frag src/shaders/hud.vert src/shaders/hud.frag src/shaders/treelines.vert src/shaders/treelines.frag src/shaders/tree.vert src/shaders/tree.frag src/shaders/leaf.vert src/shaders/leaf.frag src/shaders/grass.vert src/shaders/grass.frag src/shaders/crown.vert src/shaders/crown.frag tools/gen_shaders.sh
	./tools/gen_shaders.sh > $@

clean:
	rm -rf bin src/shaders_gen.c

.PHONY: all clean
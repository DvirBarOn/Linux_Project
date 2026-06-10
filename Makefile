# ---------- toolchain ----------
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11

# Detect platform for raylib linking
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS with Homebrew raylib
    CFLAGS += -I/opt/homebrew/include
    RAYLIB := -L/opt/homebrew/lib -lraylib -lm \
              -framework CoreVideo -framework IOKit -framework Cocoa \
              -framework GLUT -framework OpenGL
else
    # Linux
    RAYLIB := -lraylib -lm -lpthread -ldl -lrt -lX11
endif

# ---------- milestone 1: terminal Dijkstra ----------
milestone1: dijkstra

dijkstra: Dijkstra.c Dijkstra_main.c Dijkstra.h
	$(CC) $(CFLAGS) Dijkstra.c Dijkstra_main.c -o dijkstra

# ---------- milestone 2: GUI ----------
milestone2: sim

# ---------- milestone 3: GUI ----------
milestone3: sim

sim: Dijkstra.c GraphVisual.c main.c Dijkstra.h
	$(CC) $(CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim $(RAYLIB)

# ---------- milestone 4: multi-traveler + fork ----------
milestone4: sim4

sim4: Dijkstra.c GraphVisual.c main.c Dijkstra.h
	;;;;$(CC) $(CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim4 $(RAYLIB)

# ---------- milestone 5: IPC-based version ----------
milestone5: sim

# ---------- housekeeping ----------
clean:
	rm -f dijkstra sim sim4 *.o

.PHONY: milestone1 milestone2 milestone3 milestone4 milestone5 clean
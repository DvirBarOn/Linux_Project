# ---------- toolchain ----------
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11
RAYLIB  := -lraylib -lm -lpthread -ldl -lrt -lX11

# ---------- milestone 1: terminal Dijkstra ----------
milestone1: dijkstra

dijkstra: Dijkstra.c Dijkstra_main.c Dijkstra.h
	$(CC) $(CFLAGS) Dijkstra.c Dijkstra_main.c -o dijkstra

# ---------- milestone 3: single-traveler GUI ----------
milestone3: sim

sim: Dijkstra.c GraphVisual.c main.c Dijkstra.h
	$(CC) $(CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim $(RAYLIB)

# ---------- milestone 4: multi-traveler + fork ----------
# (Same sources as milestone3 in this layout; the multi-traveler logic lives
#  inside GraphVisual.c. We produce a separate binary name for clarity.)
milestone4: sim4

sim4: Dijkstra.c GraphVisual.c main.c Dijkstra.h
	$(CC) $(CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim4 $(RAYLIB)

# ---------- housekeeping ----------
clean:
	rm -f dijkstra sim sim4 *.o

.PHONY: milestone1 milestone3 milestone4 clean

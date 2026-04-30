CC = gcc
CFLAGS = -Wall -Wextra -std=c99
RAYLIB = -lraylib -lm

milestone1:
	$(CC) $(CFLAGS) main.c Dijkstra.c -o dijkstra

milestone2:
	$(CC) $(CFLAGS) main.c Dijkstra.c GraphVisual.c -o sim $(RAYLIB)

milestone3:
	$(CC) $(CFLAGS) main.c Dijkstra.c GraphVisual.c -o sim $(RAYLIB)

clean:
	rm -f dijkstra sim *.o
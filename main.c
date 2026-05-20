#include <stdio.h>
#include <stdlib.h>
#include "Dijkstra.h"

/* declaration from GraphVisual.c */
void runGraphVisualizer(const char *filename);

int main(int argc, char *argv[]) {
    const char *userArg = (argc >= 2) ? argv[1] : NULL;
    char *path = resolveGraphPath(userArg);
    if (path == NULL) {
        fprintf(stderr,
            "Could not find Graph.txt.\n"
            "Searched: command-line arg, ./Graph.txt, next to the executable, "
            "and the source directory.\n"
            "Usage: %s [path-to-graph-file]\n",
            argv[0]);
        return 1;
    }
    runGraphVisualizer(path);
    free(path);
    return 0;
}

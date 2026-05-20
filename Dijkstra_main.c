#include <stdio.h>
#include <stdlib.h>
#include "Dijkstra.h"

int main(int argc, char *argv[]) {
    Graph *graph;
    DijkstraResult result;
    int src, dest;

    if (argc != 2) {
        printf("Usage: ./dijkstra <file_name>\n");
        return 1;
    }

    graph = loadGraphFromFile(argv[1], &src, &dest);
    if (graph == NULL) {
        return 1;
    }

    result = dijkstra(graph, src, dest);
    printDijkstraResult(&result);

    freeDijkstraResult(&result);
    freeGraph(graph);
    return 0;
}

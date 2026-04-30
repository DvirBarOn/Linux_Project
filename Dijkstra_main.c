#include <stdio.h>
#include <stdlib.h>

/* declarations from Dijkstra.c */
typedef struct Edge {
    int to;
    int weight;
    struct Edge *next;
} Edge;

typedef struct {
    int numVertices;
    Edge **adj;
} Graph;

typedef struct {
    int found;
    int distance;
    int *path;
    int pathLength;
} DijkstraResult;

Graph *loadGraphFromFile(const char *filename, int *src, int *dest);
DijkstraResult dijkstra(Graph *g, int src, int dest);
void printDijkstraResult(const DijkstraResult *result);
void freeDijkstraResult(DijkstraResult *result);
void freeGraph(Graph *g);

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
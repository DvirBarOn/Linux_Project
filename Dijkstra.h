#ifndef LINUX_PROJECT_DIJSKRA_H
#define LINUX_PROJECT_DIJSKRA_H

#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include <stdio.h>  /* for FILE */

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

/* ===== graph lifecycle ===== */
Graph *createGraph(int n);
void addEdge(Graph *g, int from, int to, int w);
void freeGraph(Graph *g);

/* ===== algorithm ===== */
DijkstraResult dijkstra(Graph *g, int src, int dest);
void printDijkstraResult(const DijkstraResult *result);
void freeDijkstraResult(DijkstraResult *result);

/* ===== file loaders ===== */
/* Milestone 1/3: loads graph + a single src/dest line at the end. */
Graph *loadGraphFromFile(const char *filename, int *src, int *dest);

/* Milestone 4: loads only the graph (N M then M edges). Leaves the file
 * positioned right after the last edge so the caller can keep reading
 * traveler lines from it. Returns the open FILE* via *out_fp on success,
 * or NULL graph on failure (in which case *out_fp is also NULL). */
Graph *loadGraphOnly(const char *filename, FILE **out_fp);

#endif /* DIJKSTRA_H */



#endif //LINUX_PROJECT_DIJSKRA_H
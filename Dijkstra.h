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
Graph *loadGraphFromFile(const char *filename, int *src, int *dest);
Graph *loadGraphOnly(const char *filename, FILE **out_fp);

/* ===== path resolution =====
 * Given a (possibly NULL) user-supplied path, return a strdup'd path to a
 * readable Graph.txt-like file by searching:
 *   1. the user-supplied path, if non-NULL
 *   2. "./Graph.txt"
 *   3. <directory of the running executable>/Graph.txt
 *   4. SOURCE_DIR/Graph.txt  (set by CMake at compile time)
 * Returns NULL if nothing is found. Caller must free() the result.
 */
char *resolveGraphPath(const char *userArg);

#endif /* DIJKSTRA_H */

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

/* Allocate a graph with n vertices and initialize all adjacency lists to empty. */
Graph *createGraph(int n);

/* Insert a directed edge from -> to with weight w into the graph. */
void addEdge(Graph *g, int from, int to, int w);

/* Release all memory owned by the graph, including every adjacency-list node. */
void freeGraph(Graph *g);

/* ===== algorithm ===== */

/* Compute the shortest path from src to dest and return the full result object. */
DijkstraResult dijkstra(Graph *g, int src, int dest);

/* Print the path and total distance in the milestone 1 terminal format. */
void printDijkstraResult(const DijkstraResult *result);

/* Free any heap memory stored inside a DijkstraResult and reset its fields. */
void freeDijkstraResult(DijkstraResult *result);

/* ===== file loaders ===== */

/* Load a graph and the final src/dest pair from a milestone 1 input file. */
Graph *loadGraphFromFile(const char *filename, int *src, int *dest);

/* Load only the graph portion of the file and leave the FILE* open for further parsing. */
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

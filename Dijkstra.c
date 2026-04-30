#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define INF INT_MAX

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

Graph *createGraph(int n) {
    if (n <= 0) {
        return NULL;
    }

    Graph *g = malloc(sizeof(Graph));
    if (g == NULL) {
        return NULL;
    }

    g->numVertices = n;
    g->adj = malloc(n * sizeof(Edge *));
    if (g->adj == NULL) {
        free(g);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        g->adj[i] = NULL;
    }

    return g;
}

void addEdge(Graph *g, int from, int to, int w) {
    if (g == NULL) {
        return;
    }

    Edge *e = malloc(sizeof(Edge));
    if (e == NULL) {
        return;
    }

    e->to = to;
    e->weight = w;
    e->next = g->adj[from];
    g->adj[from] = e;
}

void freeGraph(Graph *g) {
    if (g == NULL) {
        return;
    }

    for (int i = 0; i < g->numVertices; i++) {
        Edge *cur = g->adj[i];
        while (cur != NULL) {
            Edge *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }

    free(g->adj);
    free(g);
}

int minDist(int dist[], int visited[], int n) {
    int min = INF;
    int idx = -1;

    for (int i = 0; i < n; i++) {
        if (!visited[i] && dist[i] < min) {
            min = dist[i];
            idx = i;
        }
    }

    return idx;
}

DijkstraResult dijkstra(Graph *g, int src, int dest) {
    DijkstraResult result;
    result.found = 0;
    result.distance = 0;
    result.path = NULL;
    result.pathLength = 0;

    if (g == NULL) {
        return result;
    }

    int n = g->numVertices;

    if (src < 0 || src >= n || dest < 0 || dest >= n) {
        return result;
    }

    int *dist = malloc(n * sizeof(int));
    int *visited = malloc(n * sizeof(int));
    int *parent = malloc(n * sizeof(int));

    if (dist == NULL || visited == NULL || parent == NULL) {
        free(dist);
        free(visited);
        free(parent);
        return result;
    }

    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[src] = 0;

    for (int i = 0; i < n; i++) {
        int u = minDist(dist, visited, n);

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        Edge *cur = g->adj[u];
        while (cur != NULL) {
            int v = cur->to;
            int w = cur->weight;

            if (!visited[v] && dist[u] != INF && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
            }

            cur = cur->next;
        }
    }

    if (src == dest) {
        result.found = 1;
        result.distance = 0;
        result.pathLength = 1;
        result.path = malloc(sizeof(int));
        if (result.path != NULL) {
            result.path[0] = src;
        } else {
            result.found = 0;
            result.pathLength = 0;
        }

        free(dist);
        free(visited);
        free(parent);
        return result;
    }

    if (dist[dest] == INF) {
        free(dist);
        free(visited);
        free(parent);
        return result;
    }

    int count = 0;
    int current = dest;
    while (current != -1) {
        count++;
        current = parent[current];
    }

    result.path = malloc(count * sizeof(int));
    if (result.path == NULL) {
        free(dist);
        free(visited);
        free(parent);
        return result;
    }

    current = dest;
    for (int i = count - 1; i >= 0; i--) {
        result.path[i] = current;
        current = parent[current];
    }

    result.found = 1;
    result.distance = dist[dest];
    result.pathLength = count;

    free(dist);
    free(visited);
    free(parent);

    return result;
}

void freeDijkstraResult(DijkstraResult *result) {
    if (result == NULL) {
        return;
    }

    free(result->path);
    result->path = NULL;
    result->pathLength = 0;
    result->found = 0;
    result->distance = 0;
}

void printDijkstraResult(const DijkstraResult *result) {
    if (result == NULL) {
        return;
    }

    if (!result->found) {
        printf("No path found\n");
        return;
    }

    for (int i = 0; i < result->pathLength; i++) {
        if (i > 0) {
            printf(" -> ");
        }
        printf("%d", result->path[i]);
    }

    printf("\n");
    printf("%d\n", result->distance);
}

Graph *loadGraphFromFile(const char *filename, int *src, int *dest) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("File error\n");
        return NULL;
    }

    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2) {
        fclose(fp);
        return NULL;
    }

    if (N <= 0 || M < 0) {
        printf("Invalid input\n");
        fclose(fp);
        return NULL;
    }

    Graph *g = createGraph(N);
    if (g == NULL) {
        fclose(fp);
        return NULL;
    }

    int u, v, w;
    for (int i = 0; i < M; i++) {
        if (fscanf(fp, "%d %d %d", &u, &v, &w) != 3) {
            freeGraph(g);
            fclose(fp);
            return NULL;
        }

        if (u < 0 || v < 0 || w < 0 || u >= N || v >= N) {
            printf("Invalid input\n");
            freeGraph(g);
            fclose(fp);
            return NULL;
        }

        addEdge(g, u, v, w);
    }

    if (fscanf(fp, "%d %d", src, dest) != 2) {
        freeGraph(g);
        fclose(fp);
        return NULL;
    }

    if (*src < 0 || *dest < 0 || *src >= N || *dest >= N) {
        printf("Invalid input\n");
        freeGraph(g);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return g;
}
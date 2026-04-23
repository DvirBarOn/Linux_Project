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

Graph *createGraph(int n) {
    Graph *g = malloc(sizeof(Graph));
    g->numVertices = n;
    g->adj = malloc(n * sizeof(Edge *));
    for (int i = 0; i < n; i++) {
        g->adj[i] = NULL;
    }
    return g;
}

void addEdge(Graph *g, int from, int to, int w) {
    Edge *e = malloc(sizeof(Edge));
    e->to = to;
    e->weight = w;
    e->next = g->adj[from];
    g->adj[from] = e;
}

void freeGraph(Graph *g) {
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

void printPath(int parent[], int v) {
    if (parent[v] == -1) {
        printf("%d", v);
        return;
    }

    printPath(parent, parent[v]);
    printf(" -> %d", v);
}

void dijkstra(Graph *g, int src, int dest) {
    int n = g->numVertices;

    int dist[n];
    int visited[n];
    int parent[n];

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
        printf("%d\n", src);
        printf("0\n");
        return;
    }

    if (dist[dest] == INF) {
        printf("No path found\n");
        return;
    }

    printPath(parent, dest);
    printf("\n");
    printf("%d\n", dist[dest]);
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

    Graph *g = createGraph(N);

    int u, v, w;
    for (int i = 0; i < M; i++) {
        if (fscanf(fp, "%d %d %d", &u, &v, &w) != 3) {
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

    fclose(fp);
    return g;
}
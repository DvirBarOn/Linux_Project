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
    g->adj = malloc(n * sizeof(Edge*));

    for (int i = 0; i < n; i++)
        g->adj[i] = NULL;

    return g;
}

void addEdge(Graph *g, int from, int to, int w) {
    Edge *e = malloc(sizeof(Edge));
    e->to = to;
    e->weight = w;
    e->next = g->adj[from];
    g->adj[from] = e;
}

int minDist(int dist[], int visited[], int n) {
    int min = INF, idx = -1;

    for (int i = 0; i < n; i++) {
        if (!visited[i] && dist[i] < min) {
            min = dist[i];
            idx = i;
        }
    }
    return idx;
}

void printPath(int parent[], int v) {
    if (v == -1) return;
    printPath(parent, parent[v]);
    printf("%d", v);
    if (parent[v] != -1) printf(" -> ");
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
        if (u == -1) break;

        visited[u] = 1;

        Edge *cur = g->adj[u];
        while (cur) {
            int v = cur->to;
            int w = cur->weight;

            if (!visited[v] && dist[u] != INF &&
                dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
            }
            cur = cur->next;
        }
    }

    // ❗ Cases required by assignment

    if (src == dest) {
        printf("0\n0\n");
        return;
    }

    if (dist[dest] == INF) {
        printf("No path found\n");
        return;
    }

    // print path
    printPath(parent, dest);
    printf("\n");

    // print weight
    printf("%d\n", dist[dest]);
}

int main() {
    FILE *fp = fopen("graph.txt", "r");
    if (!fp) {
        printf("File error\n");
        return 1;
    }

    int N, M;
    fscanf(fp, "%d %d", &N, &M);

    Graph *g = createGraph(N);

    int u, v, w;

    // read edges
    for (int i = 0; i < M; i++) {
        fscanf(fp, "%d %d %d", &u, &v, &w);
        addEdge(g, u, v, w); // directed
    }

    int src, dest;
    fscanf(fp, "%d %d", &src, &dest);

    fclose(fp);

    dijkstra(g, src, dest);

    return 0;
}
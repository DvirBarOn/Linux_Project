#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#if defined(__APPLE__)
#  include <mach-o/dyld.h>
#elif defined(__linux__)
#  include <linux/limits.h>
#endif

#include "Dijkstra.h"

#define INF INT_MAX

#ifndef SOURCE_DIR
#  define SOURCE_DIR "."
#endif

/* ===== helpers ===== */

static void skipCommentsAndWS(FILE *fp) {
    int c;
    for (;;) {
        do { c = fgetc(fp); } while (c != EOF && isspace(c));
        if (c == '#') {
            while (c != EOF && c != '\n') c = fgetc(fp);
            continue;
        }
        if (c != EOF) ungetc(c, fp);
        return;
    }
}

static int readInt(FILE *fp, int *out) {
    skipCommentsAndWS(fp);
    return fscanf(fp, "%d", out) == 1;
}

static int fileReadable(const char *path) {
    if (path == NULL) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    return access(path, R_OK) == 0;
}

/* Get the directory containing the running executable. Returns 1 on success. */
static int getExecutableDir(char *out, size_t outsize) {
#if defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return 0;
    char *slash = strrchr(buf, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (strlen(buf) + 1 > outsize) return 0;
    strcpy(out, buf);
    return 1;
#elif defined(__linux__)
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    buf[n] = '\0';
    char *slash = strrchr(buf, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (strlen(buf) + 1 > outsize) return 0;
    strcpy(out, buf);
    return 1;
#else
    (void)out; (void)outsize;
    return 0;
#endif
}

/* ===== path resolution ===== */

char *resolveGraphPath(const char *userArg) {
    /* 1) user-supplied */
    if (userArg && fileReadable(userArg)) {
        return strdup(userArg);
    }

    /* 2) current working directory */
    if (fileReadable("Graph.txt")) {
        return strdup("Graph.txt");
    }

    /* 3) next to the executable */
    char exedir[4096];
    if (getExecutableDir(exedir, sizeof(exedir))) {
        char candidate[4096 + 32];
        snprintf(candidate, sizeof(candidate), "%s/Graph.txt", exedir);
        if (fileReadable(candidate)) {
            return strdup(candidate);
        }
    }

    /* 4) source dir, embedded by CMake */
    {
        char candidate[4096];
        snprintf(candidate, sizeof(candidate), "%s/Graph.txt", SOURCE_DIR);
        if (fileReadable(candidate)) {
            return strdup(candidate);
        }
    }

    return NULL;
}

/* ===== graph lifecycle ===== */

Graph *createGraph(int n) {
    if (n <= 0) return NULL;
    Graph *g = malloc(sizeof(Graph));
    if (g == NULL) return NULL;
    g->numVertices = n;
    g->adj = malloc(n * sizeof(Edge *));
    if (g->adj == NULL) { free(g); return NULL; }
    for (int i = 0; i < n; i++) g->adj[i] = NULL;
    return g;
}

void addEdge(Graph *g, int from, int to, int w) {
    if (g == NULL) return;
    Edge *e = malloc(sizeof(Edge));
    if (e == NULL) return;
    e->to = to;
    e->weight = w;
    e->next = g->adj[from];
    g->adj[from] = e;
}

void freeGraph(Graph *g) {
    if (g == NULL) return;
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

/* ===== algorithm ===== */

static int minDist(int dist[], int visited[], int n) {
    int min = INF, idx = -1;
    for (int i = 0; i < n; i++) {
        if (!visited[i] && dist[i] < min) { min = dist[i]; idx = i; }
    }
    return idx;
}

DijkstraResult dijkstra(Graph *g, int src, int dest) {
    DijkstraResult result = {0, 0, NULL, 0};
    if (g == NULL) return result;
    int n = g->numVertices;
    if (src < 0 || src >= n || dest < 0 || dest >= n) return result;

    int *dist    = malloc(n * sizeof(int));
    int *visited = malloc(n * sizeof(int));
    int *parent  = malloc(n * sizeof(int));
    if (!dist || !visited || !parent) {
        free(dist); free(visited); free(parent);
        return result;
    }

    for (int i = 0; i < n; i++) { dist[i] = INF; visited[i] = 0; parent[i] = -1; }
    dist[src] = 0;

    for (int i = 0; i < n; i++) {
        int u = minDist(dist, visited, n);
        if (u == -1) break;
        visited[u] = 1;
        for (Edge *cur = g->adj[u]; cur; cur = cur->next) {
            int v = cur->to, w = cur->weight;
            if (!visited[v] && dist[u] != INF && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
            }
        }
    }

    if (src == dest) {
        result.found = 1;
        result.pathLength = 1;
        result.path = malloc(sizeof(int));
        if (result.path) result.path[0] = src;
        else { result.found = 0; result.pathLength = 0; }
        free(dist); free(visited); free(parent);
        return result;
    }

    if (dist[dest] == INF) {
        free(dist); free(visited); free(parent);
        return result;
    }

    int count = 0;
    for (int c = dest; c != -1; c = parent[c]) count++;

    result.path = malloc(count * sizeof(int));
    if (!result.path) {
        free(dist); free(visited); free(parent);
        return result;
    }

    int cur = dest;
    for (int i = count - 1; i >= 0; i--) { result.path[i] = cur; cur = parent[cur]; }
    result.found = 1;
    result.distance = dist[dest];
    result.pathLength = count;

    free(dist); free(visited); free(parent);
    return result;
}

void freeDijkstraResult(DijkstraResult *r) {
    if (!r) return;
    free(r->path);
    r->path = NULL; r->pathLength = 0; r->found = 0; r->distance = 0;
}

void printDijkstraResult(const DijkstraResult *r) {
    if (!r) return;
    if (!r->found) { printf("No path found\n"); return; }
    for (int i = 0; i < r->pathLength; i++) {
        if (i > 0) printf(" -> ");
        printf("%d", r->path[i]);
    }
    printf("\n%d\n", r->distance);
}

/* ===== loaders ===== */

Graph *loadGraphFromFile(const char *filename, int *src, int *dest) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { printf("File error\n"); return NULL; }

    int N, M;
    if (!readInt(fp, &N) || !readInt(fp, &M)) { fclose(fp); return NULL; }
    if (N <= 0 || M < 0) { printf("Invalid input\n"); fclose(fp); return NULL; }

    Graph *g = createGraph(N);
    if (!g) { fclose(fp); return NULL; }

    for (int i = 0; i < M; i++) {
        int u, v, w;
        if (!readInt(fp, &u) || !readInt(fp, &v) || !readInt(fp, &w)) {
            freeGraph(g); fclose(fp); return NULL;
        }
        if (u < 0 || v < 0 || w < 0 || u >= N || v >= N) {
            printf("Invalid input\n"); freeGraph(g); fclose(fp); return NULL;
        }
        addEdge(g, u, v, w);
    }

    if (!readInt(fp, src) || !readInt(fp, dest)) {
        freeGraph(g); fclose(fp); return NULL;
    }
    if (*src < 0 || *dest < 0 || *src >= N || *dest >= N) {
        printf("Invalid input\n"); freeGraph(g); fclose(fp); return NULL;
    }

    fclose(fp);
    return g;
}

Graph *loadGraphOnly(const char *filename, FILE **out_fp) {
    *out_fp = NULL;
    FILE *fp = fopen(filename, "r");
    if (!fp) { printf("File error\n"); return NULL; }

    int N, M;
    if (!readInt(fp, &N) || !readInt(fp, &M)) { fclose(fp); return NULL; }
    if (N <= 0 || M < 0) { printf("Invalid input\n"); fclose(fp); return NULL; }

    Graph *g = createGraph(N);
    if (!g) { fclose(fp); return NULL; }

    for (int i = 0; i < M; i++) {
        int u, v, w;
        if (!readInt(fp, &u) || !readInt(fp, &v) || !readInt(fp, &w)) {
            freeGraph(g); fclose(fp); return NULL;
        }
        if (u < 0 || v < 0 || w < 0 || u >= N || v >= N) {
            printf("Invalid input\n"); freeGraph(g); fclose(fp); return NULL;
        }
        addEdge(g, u, v, w);
    }

    *out_fp = fp;
    return g;
}

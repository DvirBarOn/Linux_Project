#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ===== declarations from Dijkstra.c ===== */

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

/* ===== visual side ===== */

#define MAX_VERTICES 15
#define NODE_RADIUS  28
#define ARROW_HEAD   12
#define ARROW_ANGLE  0.42f
#define EDGE_OFFSET  6.0f

#define BG_COLOR        (Color){15, 17, 26, 255}
#define NODE_COLOR      (Color){40, 120, 220, 255}
#define NODE_OUTLINE    (Color){100, 180, 255, 255}
#define NODE_TEXT       WHITE
#define EDGE_COLOR      (Color){180, 190, 210, 200}
#define ARROW_COLOR     (Color){220, 230, 255, 230}
#define WEIGHT_BG       (Color){30, 35, 50, 210}
#define WEIGHT_TEXT     (Color){255, 220, 80, 255}
#define TITLE_COLOR     (Color){100, 180, 255, 255}

typedef struct RawEdge {
    int from;
    int to;
    int weight;
} RawEdge;

typedef struct {
    int numVertices;
    int numEdges;
    RawEdge edges[MAX_VERTICES * MAX_VERTICES];
    Vector2 pos[MAX_VERTICES];
    int src;
    int dest;
} VisGraph;

/* ===== helpers ===== */

/* Compute the offset start/end points of an edge (same formula used everywhere) */
static void getEdgeEndpoints(VisGraph *vg, int from, int to,
                              Vector2 *start, Vector2 *end) {
    Vector2 p1 = vg->pos[from];
    Vector2 p2 = vg->pos[to];

    float dx  = p2.x - p1.x;
    float dy  = p2.y - p1.y;
    float len = sqrtf(dx * dx + dy * dy);

    float nx = dx / len;
    float ny = dy / len;
    float ox = -ny * EDGE_OFFSET;
    float oy =  nx * EDGE_OFFSET;

    start->x = p1.x + nx * NODE_RADIUS + ox;
    start->y = p1.y + ny * NODE_RADIUS + oy;

    end->x = p2.x - nx * NODE_RADIUS + ox;
    end->y = p2.y - ny * NODE_RADIUS + oy;
}

static void computeLayout(VisGraph *vg, int W, int H) {
    int n = vg->numVertices;
    float cx = W / 2.0f;
    float cy = H / 2.0f;
    float r  = fminf(W, H) * 0.36f;

    if (n == 1) {
        vg->pos[0] = (Vector2){cx, cy};
        return;
    }

    for (int i = 0; i < n; i++) {
        float angle = -PI / 2.0f + (2.0f * PI * i) / n;
        vg->pos[i].x = cx + r * cosf(angle);
        vg->pos[i].y = cy + r * sinf(angle);
    }
}

static int loadGraphVisual(const char *path, VisGraph *vg) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2) { fclose(fp); return 0; }

    if (N <= 0 || N > MAX_VERTICES || M < 0 || M > MAX_VERTICES * MAX_VERTICES) {
        fclose(fp);
        return 0;
    }

    vg->numVertices = N;
    vg->numEdges    = M;

    for (int i = 0; i < M; i++) {
        if (fscanf(fp, "%d %d %d",
                   &vg->edges[i].from,
                   &vg->edges[i].to,
                   &vg->edges[i].weight) != 3) {
            fclose(fp);
            return 0;
        }
    }

    if (fscanf(fp, "%d %d", &vg->src, &vg->dest) != 2) {
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return 1;
}

static void drawArrowHead(Vector2 tip, float dx, float dy) {
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    dx /= len;
    dy /= len;

    Vector2 b1 = {
        tip.x - ARROW_HEAD * (dx * cosf(ARROW_ANGLE) - dy * sinf(ARROW_ANGLE)),
        tip.y - ARROW_HEAD * (dy * cosf(ARROW_ANGLE) + dx * sinf(ARROW_ANGLE))
    };
    Vector2 b2 = {
        tip.x - ARROW_HEAD * (dx * cosf(ARROW_ANGLE) + dy * sinf(ARROW_ANGLE)),
        tip.y - ARROW_HEAD * (dy * cosf(ARROW_ANGLE) - dx * sinf(ARROW_ANGLE))
    };

    DrawTriangle(tip, b1, b2, ARROW_COLOR);
}

static void drawEdge(VisGraph *vg, int ei, Font font) {
    RawEdge *e = &vg->edges[ei];
    Vector2 p1 = vg->pos[e->from];
    Vector2 p2 = vg->pos[e->to];

    /* Self-loop */
    if (e->from == e->to) {
        Vector2 lc = {p1.x, p1.y - NODE_RADIUS - 18};
        DrawCircleLines((int)lc.x, (int)lc.y, 18, EDGE_COLOR);
        drawArrowHead((Vector2){p1.x + 13, p1.y - NODE_RADIUS - 4}, 1, 1);

        char buf[16];
        sprintf(buf, "%d", e->weight);
        DrawTextEx(font, buf, (Vector2){lc.x + 20, lc.y - 8}, 16, 1, WEIGHT_TEXT);
        return;
    }

    Vector2 start, end;
    getEdgeEndpoints(vg, e->from, e->to, &start, &end);

    float dx = end.x - start.x;
    float dy = end.y - start.y;

    DrawLineEx(start, end, 2.0f, EDGE_COLOR);
    drawArrowHead(end, dx, dy);

    float mx = (start.x + end.x) / 2.0f;
    float my = (start.y + end.y) / 2.0f;

    char buf[16];
    sprintf(buf, "%d", e->weight);

    Vector2 tsize = MeasureTextEx(font, buf, 15, 1);
    float pad = 4;

    DrawRectangleRounded(
        (Rectangle){
            mx - tsize.x / 2 - pad,
            my - tsize.y / 2 - pad,
            tsize.x + pad * 2,
            tsize.y + pad * 2
        },
        0.4f, 6, WEIGHT_BG
    );

    DrawTextEx(font, buf,
               (Vector2){mx - tsize.x / 2, my - tsize.y / 2},
               15, 1, WEIGHT_TEXT);
}

static void drawNode(VisGraph *vg, int i, Font font) {
    Vector2 p    = vg->pos[i];
    Color   fill = NODE_COLOR;

    if (i == vg->src)                        fill = (Color){40, 180, 100, 255};
    if (i == vg->dest)                       fill = (Color){220, 80, 80, 255};
    if (i == vg->src && i == vg->dest)       fill = (Color){180, 80, 200, 255};

    DrawCircle((int)p.x, (int)p.y, NODE_RADIUS + 5, (Color){fill.r, fill.g, fill.b, 60});
    DrawCircle((int)p.x, (int)p.y, NODE_RADIUS, fill);
    DrawCircleLines((int)p.x, (int)p.y, NODE_RADIUS, NODE_OUTLINE);

    char label[8];
    sprintf(label, "%d", i);

    Vector2 ts = MeasureTextEx(font, label, 20, 1);
    DrawTextEx(font, label,
               (Vector2){p.x - ts.x / 2, p.y - ts.y / 2},
               20, 1, NODE_TEXT);
}

static int getEdgeWeight(const VisGraph *vg, int from, int to) {
    for (int i = 0; i < vg->numEdges; i++) {
        if (vg->edges[i].from == from && vg->edges[i].to == to)
            return vg->edges[i].weight;
    }
    return 1;
}

/* ===== main entry point ===== */

void runGraphVisualizer(const char *filename) {
    const int W = 900;
    const int H = 700;

    int src, dest;
    Graph *algoGraph = loadGraphFromFile(filename, &src, &dest);

    if (algoGraph == NULL) {
        InitWindow(450, 120, "Error");
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Cannot load graph for Dijkstra", 20, 40, 20, RED);
            EndDrawing();
        }
        CloseWindow();
        return;
    }

    DijkstraResult result = dijkstra(algoGraph, src, dest);
    printf("Dijkstra result:\n");
    printDijkstraResult(&result);

    VisGraph vg = {0};
    if (!loadGraphVisual(filename, &vg)) {
        freeDijkstraResult(&result);
        freeGraph(algoGraph);

        InitWindow(400, 120, "Error");
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Cannot open graph file", 30, 40, 20, RED);
            EndDrawing();
        }
        CloseWindow();
        return;
    }

    InitWindow(W, H, "Graph Visualizer");
    SetTargetFPS(60);

    computeLayout(&vg, W, H);

    Font font = GetFontDefault();

    int   isPlaying      = 0;
    int   currentSegment = 0;
    int   currentStep    = 0;
    int   totalSteps     = 1;
    float stepTimer      = 0.0f;

    int   isWaitingAtNode = 0;
    float waitTimer       = 0.0f;

    if (result.found && result.pathLength > 1) {
        totalSteps = getEdgeWeight(&vg, result.path[0], result.path[1]);
        if (totalSteps <= 0) totalSteps = 1;
    }

    while (!WindowShouldClose()) {

        /* ---- animation update ---- */
        if (isPlaying && result.found && result.pathLength > 1) {

            if (isWaitingAtNode) {
                waitTimer += GetFrameTime();
                if (waitTimer >= 1.0f) {
                    isWaitingAtNode = 0;
                    waitTimer       = 0.0f;
                }
            } else {
                stepTimer += GetFrameTime();
                if (stepTimer >= 0.3f) {
                    stepTimer = 0.0f;
                    currentStep++;

                    if (currentStep >= totalSteps) {
                        currentStep = 0;
                        currentSegment++;

                        if (currentSegment >= result.pathLength - 1) {
                            currentSegment = result.pathLength - 1;
                            isPlaying      = 0;
                        } else {
                            totalSteps = getEdgeWeight(&vg,
                                result.path[currentSegment],
                                result.path[currentSegment + 1]);
                            if (totalSteps <= 0) totalSteps = 1;

                            isWaitingAtNode = 1;
                            waitTimer       = 0.0f;
                        }
                    }
                }
            }
        }

        /* ---- draw ---- */
        BeginDrawing();
        ClearBackground(BG_COLOR);

        /* dot grid background */
        for (int x = 0; x < W; x += 40)
            for (int y = 0; y < H; y += 40)
                DrawPixel(x, y, (Color){60, 70, 100, 80});

        /* title */
        const char *title = "Graph Visualizer";
        Vector2 ts = MeasureTextEx(font, title, 22, 1);
        DrawTextEx(font, title, (Vector2){(W - ts.x) / 2, 14}, 22, 1, TITLE_COLOR);

        /* legend */
        DrawCircle(30, H - 60, 10, (Color){40, 180, 100, 255});
        DrawTextEx(font, "Source", (Vector2){46, H - 68}, 14, 1, WHITE);
        DrawCircle(30, H - 35, 10, (Color){220, 80, 80, 255});
        DrawTextEx(font, "Dest",   (Vector2){46, H - 43}, 14, 1, WHITE);

        /* edges then nodes */
        for (int i = 0; i < vg.numEdges; i++)    drawEdge(&vg, i, font);
        for (int i = 0; i < vg.numVertices; i++) drawNode(&vg, i, font);

        /* ---- entity (yellow dot) ---- */
        if (result.found && result.pathLength > 0) {
            Vector2 entityPos;

            if (result.pathLength == 1) {
                /* Only one node in path */
                entityPos = vg.pos[result.path[0]];

            } else if (currentSegment >= result.pathLength - 1) {
                /* Arrived at destination */
                entityPos = vg.pos[result.path[result.pathLength - 1]];

            } else {
                /* Interpolate along the exact same offset line used by drawEdge() */
                int fromVertex = result.path[currentSegment];
                int toVertex   = result.path[currentSegment + 1];

                Vector2 start, end;
                getEdgeEndpoints(&vg, fromVertex, toVertex, &start, &end);

                float t = (totalSteps > 0)
                          ? (float)currentStep / (float)totalSteps
                          : 0.0f;

                entityPos.x = start.x + (end.x - start.x) * t;
                entityPos.y = start.y + (end.y - start.y) * t;
            }

            /* Draw dot directly on entityPos — no manual offset */
            DrawCircle((int)entityPos.x, (int)entityPos.y, 10, YELLOW);
            DrawTextEx(font, "Entity",
                       (Vector2){entityPos.x + 14, entityPos.y - 8},
                       14, 1, WHITE);
        }

        /* destination reached banner */
        if (result.found && result.pathLength > 0 &&
            currentSegment >= result.pathLength - 1) {
            DrawTextEx(font, "Reached destination!",
                       (Vector2){W / 2.0f - 110, H - 40},
                       22, 1, YELLOW);
        }

        /* play / stop button */
        Rectangle button = {W - 140, 20, 100, 36};

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(), button)) {

            /* restart if animation finished */
            if (!isPlaying && result.found && result.pathLength > 1 &&
                currentSegment >= result.pathLength - 1) {

                currentSegment  = 0;
                currentStep     = 0;
                stepTimer       = 0.0f;
                isWaitingAtNode = 0;
                waitTimer       = 0.0f;

                totalSteps = getEdgeWeight(&vg, result.path[0], result.path[1]);
                if (totalSteps <= 0) totalSteps = 1;
            }

            isPlaying = !isPlaying;
        }

        DrawRectangleRounded(button, 0.3f, 8, isPlaying ? ORANGE : DARKGREEN);
        DrawTextEx(font,
                   isPlaying ? "Stop" : "Play",
                   (Vector2){button.x + 24, button.y + 8},
                   20, 1, WHITE);

        EndDrawing();
    }

    CloseWindow();
    freeDijkstraResult(&result);
    freeGraph(algoGraph);
}
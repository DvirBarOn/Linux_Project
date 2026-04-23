#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MAX_VERTICES 15
#define NODE_RADIUS  28
#define ARROW_HEAD   12
#define ARROW_ANGLE  0.42f   /* radians ~24° */

/* ── Colour palette ─────────────────────────────────────────── */
#define BG_COLOR        (Color){15,  17,  26,  255}
#define NODE_COLOR      (Color){40,  120, 220, 255}
#define NODE_OUTLINE    (Color){100, 180, 255, 255}
#define NODE_TEXT       WHITE
#define EDGE_COLOR      (Color){180, 190, 210, 200}
#define ARROW_COLOR     (Color){220, 230, 255, 230}
#define WEIGHT_BG       (Color){30,  35,  50,  210}
#define WEIGHT_TEXT     (Color){255, 220, 80,  255}
#define TITLE_COLOR     (Color){100, 180, 255, 255}

/* ── Data structures ────────────────────────────────────────── */
typedef struct RawEdge { int from, to, weight; } RawEdge;

typedef struct {
    int       numVertices;
    int       numEdges;
    RawEdge   edges[MAX_VERTICES * MAX_VERTICES];
    Vector2   pos[MAX_VERTICES];   /* screen positions */
    int       src, dest;           /* Dijkstra endpoints from file */
} VisGraph;

/* ── Layout: circular arrangement, centred in the window ────── */
static void computeLayout(VisGraph *vg, int W, int H) {
    int  n  = vg->numVertices;
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

/* ── File parsing ───────────────────────────────────────────── */
static int loadGraph(const char *path, VisGraph *vg) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    int N, M;
    if (fscanf(fp, "%d %d", &N, &M) != 2) { fclose(fp); return 0; }
    vg->numVertices = N;
    vg->numEdges    = M;

    for (int i = 0; i < M; i++)
        fscanf(fp, "%d %d %d",
               &vg->edges[i].from,
               &vg->edges[i].to,
               &vg->edges[i].weight);

    fscanf(fp, "%d %d", &vg->src, &vg->dest);
    fclose(fp);
    return 1;
}

/* ── Draw a filled arrowhead at point (tx,ty) pointing in direction (dx,dy) */
static void drawArrowHead(Vector2 tip, float dx, float dy) {
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.001f) return;
    dx /= len; dy /= len;

    /* two base points of the triangle */
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

/* ── Draw a single directed edge ─────────────────────────────── */
static void drawEdge(VisGraph *vg, int ei, Font font) {
    RawEdge *e  = &vg->edges[ei];
    Vector2  p1 = vg->pos[e->from];
    Vector2  p2 = vg->pos[e->to];

    if (e->from == e->to) {
        /* self-loop: small circle above the node */
        Vector2 lc = {p1.x, p1.y - NODE_RADIUS - 18};
        DrawCircleLines((int)lc.x, (int)lc.y, 18, EDGE_COLOR);
        drawArrowHead((Vector2){p1.x + 13, p1.y - NODE_RADIUS - 4}, 1, 1);
        char buf[16]; sprintf(buf, "%d", e->weight);
        DrawTextEx(font, buf,
                   (Vector2){lc.x + 20, lc.y - 8},
                   16, 1, WEIGHT_TEXT);
        return;
    }

    /* direction vector */
    float dx = p2.x - p1.x, dy = p2.y - p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    float nx = dx/len, ny = dy/len;

    /* offset so lines between the same pair don't overlap (parallel shift) */
    /* perpendicular offset for bidirectional edges */
    float ox = -ny * 6.0f, oy = nx * 6.0f;

    Vector2 start = { p1.x + nx * NODE_RADIUS + ox,
                      p1.y + ny * NODE_RADIUS + oy };
    Vector2 end   = { p2.x - nx * NODE_RADIUS + ox,
                      p2.y - ny * NODE_RADIUS + oy };

    DrawLineEx(start, end, 2.0f, EDGE_COLOR);
    drawArrowHead(end, nx, ny);

    /* weight label — midpoint, with a small background rectangle */
    float mx = (start.x + end.x) / 2.0f;
    float my = (start.y + end.y) / 2.0f;

    char buf[16]; sprintf(buf, "%d", e->weight);
    Vector2 tsize = MeasureTextEx(font, buf, 15, 1);
    float pad = 4;
    DrawRectangleRounded(
        (Rectangle){mx - tsize.x/2 - pad, my - tsize.y/2 - pad,
                    tsize.x + pad*2, tsize.y + pad*2},
        0.4f, 6, WEIGHT_BG);
    DrawTextEx(font, buf,
               (Vector2){mx - tsize.x/2, my - tsize.y/2},
               15, 1, WEIGHT_TEXT);
}

/* ── Draw all nodes ─────────────────────────────────────────── */
static void drawNode(VisGraph *vg, int i, Font font) {
    Vector2 p   = vg->pos[i];
    Color   fill = NODE_COLOR;

    /* highlight source / destination */
    if (i == vg->src)  fill = (Color){40, 180, 100, 255};
    if (i == vg->dest) fill = (Color){220, 80,  80,  255};
    if (i == vg->src && i == vg->dest) fill = (Color){180, 80, 200, 255};

    /* glow ring */
    DrawCircle((int)p.x, (int)p.y, NODE_RADIUS + 5,
               (Color){fill.r, fill.g, fill.b, 60});
    /* filled circle */
    DrawCircle((int)p.x, (int)p.y, NODE_RADIUS, fill);
    /* outline */
    DrawCircleLines((int)p.x, (int)p.y, NODE_RADIUS, NODE_OUTLINE);

    /* vertex id */
    char label[8]; sprintf(label, "%d", i);
    Vector2 ts = MeasureTextEx(font, label, 20, 1);
    DrawTextEx(font, label,
               (Vector2){p.x - ts.x/2, p.y - ts.y/2},
               20, 1, NODE_TEXT);
}

/* ── Main ───────────────────────────────────────────────────── */
void runGraphVisualizer(void) {
    const int W = 900, H = 700;

    VisGraph vg = {0};
    if (!loadGraph("Graph.txt", &vg)) {
        InitWindow(400, 120, "Error");
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Cannot open Graph.txt", 30, 40, 20, RED);
            EndDrawing();
        }
        CloseWindow();
        return;
    }

    InitWindow(W, H, "Graph Visualizer");
    SetTargetFPS(60);

    computeLayout(&vg, W, H);

    Font font = GetFontDefault();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BG_COLOR);

        for (int x = 0; x < W; x += 40)
            for (int y = 0; y < H; y += 40)
                DrawPixel(x, y, (Color){60, 70, 100, 80});

        const char *title = "Graph Visualizer";
        Vector2 ts = MeasureTextEx(font, title, 22, 1);
        DrawTextEx(font, title,
                   (Vector2){(W - ts.x)/2, 14},
                   22, 1, TITLE_COLOR);

        DrawCircle(30, H - 60, 10, (Color){40,180,100,255});
        DrawTextEx(font, "Source", (Vector2){46, H - 68}, 14, 1, WHITE);
        DrawCircle(30, H - 35, 10, (Color){220,80,80,255});
        DrawTextEx(font, "Dest", (Vector2){46, H - 43}, 14, 1, WHITE);

        for (int i = 0; i < vg.numEdges; i++)
            drawEdge(&vg, i, font);

        for (int i = 0; i < vg.numVertices; i++)
            drawNode(&vg, i, font);

        EndDrawing();
    }

    CloseWindow();
}
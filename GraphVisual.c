/* Enable POSIX APIs (kill, waitpid, SIGTERM, pause) under -std=c11. */
#define _POSIX_C_SOURCE 200809L

#include "raylib.h"
#include "Dijkstra.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

/* Skip whitespace and '#' comment lines in the open file. */
static void skipCommentsWS(FILE *fp) {
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

/* ===== visual constants ===== */
#define MAX_VERTICES   15
#define MAX_TRAVELERS  16
#define NODE_RADIUS    28
#define ARROW_HEAD     12
#define ARROW_ANGLE    0.42f
#define EDGE_OFFSET    6.0f

#define BG_COLOR     (Color){15, 17, 26, 255}
#define NODE_COLOR   (Color){40, 120, 220, 255}
#define NODE_OUTLINE (Color){100, 180, 255, 255}
#define NODE_TEXT    WHITE
#define EDGE_COLOR   (Color){180, 190, 210, 200}
#define ARROW_COLOR  (Color){220, 230, 255, 230}
#define WEIGHT_BG    (Color){30, 35, 50, 210}
#define WEIGHT_TEXT  (Color){255, 220, 80, 255}
#define TITLE_COLOR  (Color){100, 180, 255, 255}

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
} VisGraph;

typedef struct {
    int src;
    int dest;
    DijkstraResult result;     /* path computed by parent before fork */
    pid_t pid;                 /* child process id */
    Color color;               /* unique color per traveler */

    /* per-traveler animation state */
    int currentSegment;
    int currentStep;
    int totalSteps;
    float stepTimer;
    int isWaitingAtNode;
    float waitTimer;
    int arrived;               /* set once destination reached */
    int signaled;              /* set once parent has sent SIGTERM to child */
} Traveler;

/* ===== helpers ===== */

static void getEdgeEndpoints(VisGraph *vg, int from, int to,
                             Vector2 *start, Vector2 *end) {
    Vector2 p1 = vg->pos[from];
    Vector2 p2 = vg->pos[to];
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) { *start = p1; *end = p2; return; }
    float nx = dx / len;
    float ny = dy / len;
    float ox = -ny * EDGE_OFFSET;
    float oy = nx * EDGE_OFFSET;
    start->x = p1.x + nx * NODE_RADIUS + ox;
    start->y = p1.y + ny * NODE_RADIUS + oy;
    end->x = p2.x - nx * NODE_RADIUS + ox;
    end->y = p2.y - ny * NODE_RADIUS + oy;
}

static void computeLayout(VisGraph *vg, int W, int H) {
    int n = vg->numVertices;
    float cx = W / 2.0f;
    float cy = H / 2.0f;
    float r = fminf(W, H) * 0.36f;

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

/* Read only the graph part into VisGraph (no src/dest, no travelers). */
static int loadVisGraph(const char *path, VisGraph *vg) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    int N, M;
    skipCommentsWS(fp);
    if (fscanf(fp, "%d %d", &N, &M) != 2) { fclose(fp); return 0; }
    if (N <= 0 || N > MAX_VERTICES || M < 0 || M > MAX_VERTICES * MAX_VERTICES) {
        fclose(fp);
        return 0;
    }

    vg->numVertices = N;
    vg->numEdges = M;
    for (int i = 0; i < M; i++) {
        skipCommentsWS(fp);
        if (fscanf(fp, "%d %d %d",
                   &vg->edges[i].from,
                   &vg->edges[i].to,
                   &vg->edges[i].weight) != 3) {
            fclose(fp);
            return 0;
        }
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

static void drawNode(VisGraph *vg, int i, Font font,
                     Traveler *travelers, int numTravelers) {
    Vector2 p = vg->pos[i];
    Color fill = NODE_COLOR;

    /* If any traveler uses this node as src/dest, tint accordingly.
     * Source-tint wins over dest-tint if a node is both, and we keep
     * the simple green/red so the parent test criteria still hold. */
    int isSrc = 0, isDest = 0;
    for (int t = 0; t < numTravelers; t++) {
        if (travelers[t].src == i)  isSrc = 1;
        if (travelers[t].dest == i) isDest = 1;
    }
    if (isSrc && isDest) fill = (Color){180, 80, 200, 255};
    else if (isSrc)      fill = (Color){40, 180, 100, 255};
    else if (isDest)     fill = (Color){220, 80, 80, 255};

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

/* Distinct, easy-to-tell-apart colors for travelers. */
static Color travelerColor(int idx) {
    static const Color palette[] = {
        {255, 220,  60, 255},   /* yellow      */
        {  0, 200, 255, 255},   /* cyan        */
        {255, 120, 200, 255},   /* pink        */
        {120, 255, 120, 255},   /* lime green  */
        {255, 140,  40, 255},   /* orange      */
        {200, 120, 255, 255},   /* purple      */
        {255,  80,  80, 255},   /* red         */
        { 80, 200, 160, 255},   /* teal        */
        {220, 220, 220, 255},   /* light gray  */
        {255, 200, 150, 255},   /* peach       */
        {150, 200, 255, 255},   /* sky         */
        {200, 255,  80, 255},   /* yellow-green*/
        {255, 100, 150, 255},   /* rose        */
        {100, 255, 220, 255},   /* mint        */
        {180, 180, 255, 255},   /* lavender    */
        {255, 180,  80, 255},   /* amber       */
    };
    int n = (int)(sizeof(palette) / sizeof(palette[0]));
    return palette[idx % n];
}

/* ===== child process body =====
 * Each child prints "[PID] started" then sleeps until parent signals it. */
static void childMain(void) {
    printf("[%d] started\n", (int)getpid());
    fflush(stdout);

    /* Wait for any signal. pause() returns -1 with errno=EINTR when a
     * signal arrives — at which point we exit cleanly. */
    while (1) {
        pause();
        /* If we get here it means a signal was delivered. Time to go. */
        break;
    }
    exit(0);
}

/* ===== main entry point ===== */
void runGraphVisualizer(const char *filename) {
    const int W = 900;
    const int H = 700;

    /* ---- Step 1: load the graph ---- */
    FILE *fp = NULL;
    Graph *algoGraph = loadGraphOnly(filename, &fp);
    if (algoGraph == NULL) {
        InitWindow(450, 120, "Error");
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Cannot load graph", 30, 40, 20, RED);
            EndDrawing();
        }
        CloseWindow();
        return;
    }

    /* ---- Step 2: read traveler count and pairs ---- */
    int numTravelers = 0;
    skipCommentsWS(fp);
    if (fscanf(fp, "%d", &numTravelers) != 1
        || numTravelers <= 0
        || numTravelers > MAX_TRAVELERS) {
        printf("Invalid traveler count\n");
        fclose(fp);
        freeGraph(algoGraph);
        return;
    }

    Traveler *travelers = calloc(numTravelers, sizeof(Traveler));
    if (travelers == NULL) {
        fclose(fp);
        freeGraph(algoGraph);
        return;
    }

    for (int i = 0; i < numTravelers; i++) {
        int s, d;
        skipCommentsWS(fp);
        if (fscanf(fp, "%d %d", &s, &d) != 2) {
            printf("Invalid traveler line %d\n", i + 1);
            fclose(fp);
            free(travelers);
            freeGraph(algoGraph);
            return;
        }
        if (s < 0 || s >= algoGraph->numVertices
            || d < 0 || d >= algoGraph->numVertices) {
            printf("Traveler %d out of range\n", i + 1);
            fclose(fp);
            free(travelers);
            freeGraph(algoGraph);
            return;
        }
        travelers[i].src = s;
        travelers[i].dest = d;
        travelers[i].color = travelerColor(i);
    }
    fclose(fp);

    /* ---- Step 3: parent computes each traveler's path BEFORE forking ---- */
    for (int i = 0; i < numTravelers; i++) {
        travelers[i].result = dijkstra(algoGraph,
                                       travelers[i].src,
                                       travelers[i].dest);
        printf("Traveler %d (%d -> %d): ",
               i + 1, travelers[i].src, travelers[i].dest);
        printDijkstraResult(&travelers[i].result);
    }

    /* ---- Step 4: fork one child per traveler ----
     * Flush stdout first: any pending output in the parent's buffer would
     * otherwise be inherited (and re-printed) by every child. */
    fflush(stdout);
    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            /* best effort: kill any already-spawned children before bailing */
            for (int k = 0; k < i; k++) {
                if (travelers[k].pid > 0) {
                    kill(travelers[k].pid, SIGTERM);
                    waitpid(travelers[k].pid, NULL, 0);
                }
            }
            for (int k = 0; k < numTravelers; k++)
                freeDijkstraResult(&travelers[k].result);
            free(travelers);
            freeGraph(algoGraph);
            return;
        }
        if (pid == 0) {
            /* --- CHILD ---
             * Free the parent's heap state we don't need; we won't touch it. */
            childMain();
            /* never returns */
        }
        /* --- PARENT --- */
        travelers[i].pid = pid;
    }

    /* ---- Step 5: load visual graph + run animation ---- */
    VisGraph vg = {0};
    if (!loadVisGraph(filename, &vg)) {
        /* shouldn't happen since we just read it, but be defensive */
        for (int i = 0; i < numTravelers; i++) {
            if (travelers[i].pid > 0) {
                kill(travelers[i].pid, SIGTERM);
                waitpid(travelers[i].pid, NULL, 0);
            }
            freeDijkstraResult(&travelers[i].result);
        }
        free(travelers);
        freeGraph(algoGraph);
        return;
    }

    InitWindow(W, H, "Graph Visualizer - Multi Traveler");
    SetTargetFPS(60);
    computeLayout(&vg, W, H);
    Font font = GetFontDefault();

    /* init per-traveler animation state */
    for (int i = 0; i < numTravelers; i++) {
        Traveler *t = &travelers[i];
        t->currentSegment = 0;
        t->currentStep = 0;
        t->stepTimer = 0.0f;
        t->isWaitingAtNode = 0;
        t->waitTimer = 0.0f;
        t->arrived = 0;
        t->signaled = 0;

        if (t->result.found && t->result.pathLength > 1) {
            t->totalSteps = getEdgeWeight(&vg, t->result.path[0], t->result.path[1]);
            if (t->totalSteps <= 0) t->totalSteps = 1;
        } else {
            t->totalSteps = 1;
            /* If path not found or only one node, mark as arrived immediately. */
            if (!t->result.found || t->result.pathLength <= 1) {
                t->arrived = 1;
            }
        }
    }

    int isPlaying = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ---- update every traveler independently ---- */
        if (isPlaying) {
            for (int i = 0; i < numTravelers; i++) {
                Traveler *t = &travelers[i];
                if (t->arrived) continue;
                if (!t->result.found || t->result.pathLength <= 1) {
                    t->arrived = 1;
                    continue;
                }

                if (t->isWaitingAtNode) {
                    t->waitTimer += dt;
                    if (t->waitTimer >= 1.0f) {
                        t->isWaitingAtNode = 0;
                        t->waitTimer = 0.0f;
                    }
                } else {
                    t->stepTimer += dt;
                    if (t->stepTimer >= 0.3f) {
                        t->stepTimer = 0.0f;
                        t->currentStep++;
                        if (t->currentStep >= t->totalSteps) {
                            t->currentStep = 0;
                            t->currentSegment++;
                            if (t->currentSegment >= t->result.pathLength - 1) {
                                t->currentSegment = t->result.pathLength - 1;
                                t->arrived = 1;
                            } else {
                                t->totalSteps = getEdgeWeight(
                                    &vg,
                                    t->result.path[t->currentSegment],
                                    t->result.path[t->currentSegment + 1]);
                                if (t->totalSteps <= 0) t->totalSteps = 1;
                                t->isWaitingAtNode = 1;
                                t->waitTimer = 0.0f;
                            }
                        }
                    }
                }
            }
        }

        /* ---- when a traveler arrives, signal its child to terminate ---- */
        for (int i = 0; i < numTravelers; i++) {
            Traveler *t = &travelers[i];
            if (t->arrived && !t->signaled && t->pid > 0) {
                kill(t->pid, SIGTERM);
                waitpid(t->pid, NULL, 0);
                t->signaled = 1;
                printf("Traveler %d (PID %d) reached destination, terminated\n",
                       i + 1, (int)t->pid);
                fflush(stdout);
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
        const char *title = "Graph Visualizer - Milestone 4";
        Vector2 ts = MeasureTextEx(font, title, 22, 1);
        DrawTextEx(font, title, (Vector2){(W - ts.x) / 2, 14}, 22, 1, TITLE_COLOR);

        /* edges then nodes */
        for (int i = 0; i < vg.numEdges; i++) drawEdge(&vg, i, font);
        for (int i = 0; i < vg.numVertices; i++)
            drawNode(&vg, i, font, travelers, numTravelers);

        /* ---- draw each traveler's dot ---- */
        for (int i = 0; i < numTravelers; i++) {
            Traveler *t = &travelers[i];
            if (!t->result.found || t->result.pathLength <= 0) continue;

            Vector2 entityPos;
            if (t->result.pathLength == 1) {
                entityPos = vg.pos[t->result.path[0]];
            } else if (t->arrived) {
                entityPos = vg.pos[t->result.path[t->result.pathLength - 1]];
            } else {
                int fromV = t->result.path[t->currentSegment];
                int toV   = t->result.path[t->currentSegment + 1];
                Vector2 start, end;
                getEdgeEndpoints(&vg, fromV, toV, &start, &end);
                float ratio = (t->totalSteps > 0)
                    ? (float)t->currentStep / (float)t->totalSteps
                    : 0.0f;
                entityPos.x = start.x + (end.x - start.x) * ratio;
                entityPos.y = start.y + (end.y - start.y) * ratio;
            }

            /* halo + filled circle so each color reads even on overlap */
            DrawCircle((int)entityPos.x, (int)entityPos.y, 14,
                       (Color){t->color.r, t->color.g, t->color.b, 80});
            DrawCircle((int)entityPos.x, (int)entityPos.y, 9, t->color);

            char lab[16];
            sprintf(lab, "T%d", i + 1);
            DrawTextEx(font, lab,
                       (Vector2){entityPos.x + 14, entityPos.y - 8},
                       14, 1, WHITE);
        }

        /* ---- side panel: traveler status ---- */
        int panelX = 20;
        int panelY = 70;
        DrawTextEx(font, "Travelers:",
                   (Vector2){panelX, panelY}, 16, 1, WHITE);
        for (int i = 0; i < numTravelers; i++) {
            Traveler *t = &travelers[i];
            int yy = panelY + 24 + i * 22;
            DrawCircle(panelX + 8, yy + 8, 7, t->color);
            char line[80];
            snprintf(line, sizeof(line), "T%d  %d -> %d  %s",
                     i + 1, t->src, t->dest,
                     t->arrived ? "[arrived]" :
                     (t->result.found ? "" : "[no path]"));
            DrawTextEx(font, line, (Vector2){panelX + 22, yy}, 14, 1, WHITE);
        }

        /* "all done" banner */
        int allDone = 1;
        for (int i = 0; i < numTravelers; i++) {
            if (!travelers[i].arrived) { allDone = 0; break; }
        }
        if (allDone) {
            DrawTextEx(font, "All travelers arrived!",
                       (Vector2){W / 2.0f - 120, H - 40},
                       22, 1, YELLOW);
        }

        /* play / stop button */
        Rectangle button = {W - 140, 20, 100, 36};
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(), button)) {

            /* if everyone finished, clicking restarts the animation but
             * does NOT respawn children — they're already terminated. */
            if (!isPlaying && allDone) {
                for (int i = 0; i < numTravelers; i++) {
                    Traveler *t = &travelers[i];
                    if (!t->result.found || t->result.pathLength <= 1) continue;
                    t->currentSegment = 0;
                    t->currentStep = 0;
                    t->stepTimer = 0.0f;
                    t->isWaitingAtNode = 0;
                    t->waitTimer = 0.0f;
                    t->arrived = 0;
                    /* leave signaled=1; children are already gone */
                    t->totalSteps = getEdgeWeight(&vg,
                        t->result.path[0], t->result.path[1]);
                    if (t->totalSteps <= 0) t->totalSteps = 1;
                }
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

    /* ---- cleanup: make sure no children outlive us ---- */
    for (int i = 0; i < numTravelers; i++) {
        Traveler *t = &travelers[i];
        if (t->pid > 0 && !t->signaled) {
            kill(t->pid, SIGTERM);
            waitpid(t->pid, NULL, 0);
        }
        freeDijkstraResult(&t->result);
    }
    free(travelers);
    freeGraph(algoGraph);
}

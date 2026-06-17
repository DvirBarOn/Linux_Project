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
#include <fcntl.h>
#include <time.h>

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

#define BG_COLOR       (Color){15, 17, 26, 255}
#define NODE_COLOR     (Color){40, 120, 220, 255}
#define NODE_OUTLINE   (Color){100, 180, 255, 255}
#define NODE_TEXT      WHITE
#define EDGE_COLOR     (Color){180, 190, 210, 200}
#define ARROW_COLOR    (Color){220, 230, 255, 230}
#define WEIGHT_BG      (Color){30, 35, 50, 210}
#define WEIGHT_TEXT    (Color){255, 220, 80, 255}
#define TITLE_COLOR    (Color){100, 180, 255, 255}

#define TOP_BAR_HEIGHT 80
#define BUTTON_W       120
#define BUTTON_H       40
#define BUTTON_GAP     16

/* edge movement timing */
#define SECONDS_PER_WEIGHT    0.4
#define MIN_EDGE_DURATION     0.4

/* milestone 6/7 */
#define NODE_STAY_SECONDS       1.0
#define COLLECTION_WINDOW_SECONDS 0.20

typedef enum {
    SCHED_FCFS,
    SCHED_SJF
} SchedulerType;

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

typedef enum {
    MSG_REQUEST_NODE,
    MSG_ENTERED_NODE,
    MSG_LEAVING_NODE,
    MSG_FINISHED
} MessageType;

typedef struct {
    MessageType type;
    pid_t pid;
    int travelerIndex;
    int currentNode;
    int nextNode;
    int nextEdgeWeight;
    int isDestination;
} TravelerMessage;

typedef struct {
    int travelerIndex;
    pid_t pid;
    int currentNode;
    int nextNode;
    int nextEdgeWeight;
    long long arrivalOrder;
} WaitingTraveler;

typedef struct {
    int occupiedBy;                  /* -1 if free */
    WaitingTraveler queue[MAX_TRAVELERS];
    int queueSize;

    int collecting;
    double collectionStartTime;
} NodeQueue;

typedef struct {
    int src;
    int dest;
    DijkstraResult result;
    pid_t pid;
    Color color;

    int arrived;
    int signaled;

    int pipeFd[2];        /* child -> parent */
    int controlPipe[2];   /* parent -> child */

    int currentNode;
    int nextNode;
    int finished;
    int started;

    int drawFromNode;
    int drawToNode;
    double moveStartTime;
    int moving;
    double pausedProgress;
    double edgeDuration;

    int waitingForNode;
    int insideNode;
    int waitFromNode;
} Traveler;

/* ===== helpers ===== */

static const char *schedulerName(SchedulerType scheduler) {
    return (scheduler == SCHED_SJF) ? "SJF" : "FCFS";
}

static void initNodeQueues(NodeQueue *nodeQueues, int numVertices) {
    for (int i = 0; i < numVertices; i++) {
        nodeQueues[i].occupiedBy = -1;
        nodeQueues[i].queueSize = 0;
        nodeQueues[i].collecting = 0;
        nodeQueues[i].collectionStartTime = 0.0;
    }
}

static void enqueueTraveler(NodeQueue *nodeQueues, const WaitingTraveler *wt) {
    int node = wt->currentNode;

    if (node < 0 || node >= MAX_VERTICES) return;
    if (nodeQueues[node].queueSize >= MAX_TRAVELERS) return;

    nodeQueues[node].queue[nodeQueues[node].queueSize] = *wt;
    nodeQueues[node].queueSize++;
}

static int pickNextTravelerIndex(const NodeQueue *nodeQueues, int node, SchedulerType scheduler) {
    if (node < 0 || node >= MAX_VERTICES) return -1;
    if (nodeQueues[node].queueSize <= 0) return -1;

    int best = 0;

    for (int i = 1; i < nodeQueues[node].queueSize; i++) {
        const WaitingTraveler *cand = &nodeQueues[node].queue[i];
        const WaitingTraveler *cur  = &nodeQueues[node].queue[best];

        if (scheduler == SCHED_FCFS) {
            if (cand->arrivalOrder < cur->arrivalOrder) {
                best = i;
            }
        } else {
            if (cand->nextEdgeWeight < cur->nextEdgeWeight) {
                best = i;
            } else if (cand->nextEdgeWeight == cur->nextEdgeWeight &&
                       cand->arrivalOrder < cur->arrivalOrder) {
                best = i;
            }
        }
    }

    return best;
}

static WaitingTraveler removeWaitingTraveler(NodeQueue *nodeQueues, int node, int idx) {
    WaitingTraveler removed = {0};

    if (node < 0 || node >= MAX_VERTICES) return removed;
    if (idx < 0 || idx >= nodeQueues[node].queueSize) return removed;

    removed = nodeQueues[node].queue[idx];

    for (int i = idx; i < nodeQueues[node].queueSize - 1; i++) {
        nodeQueues[node].queue[i] = nodeQueues[node].queue[i + 1];
    }

    nodeQueues[node].queueSize--;
    return removed;
}

static int sendTravelerMessage(int fd, const TravelerMessage *msg) {
    ssize_t n = write(fd, msg, sizeof(*msg));
    return (n == (ssize_t)sizeof(*msg));
}

static int sendEnterApproval(int fd, int node) {
    ssize_t n = write(fd, &node, sizeof(node));
    return (n == (ssize_t)sizeof(node));
}

static int waitForEnterApproval(int fd, int expectedNode) {
    int approvedNode = -1;
    ssize_t n = read(fd, &approvedNode, sizeof(approvedNode));
    if (n != (ssize_t)sizeof(approvedNode)) {
        return 0;
    }
    return (approvedNode == expectedNode);
}

static void startCollectionWindow(NodeQueue *nodeQueues, int node, double now) {
    if (node < 0 || node >= MAX_VERTICES) return;
    if (nodeQueues[node].occupiedBy != -1) return;
    if (nodeQueues[node].queueSize <= 0) return;
    if (nodeQueues[node].collecting) return;

    nodeQueues[node].collecting = 1;
    nodeQueues[node].collectionStartTime = now;
}

static void tryDispatchNextTraveler(NodeQueue *nodeQueues,
                                    Traveler *travelers,
                                    int numTravelers,
                                    int node,
                                    SchedulerType scheduler) {
    if (node < 0 || node >= MAX_VERTICES) return;
    if (nodeQueues[node].occupiedBy != -1) return;
    if (nodeQueues[node].queueSize <= 0) return;

    int idx = pickNextTravelerIndex(nodeQueues, node, scheduler);
    if (idx < 0) return;

    WaitingTraveler wt = removeWaitingTraveler(nodeQueues, node, idx);

    if (wt.travelerIndex < 0 || wt.travelerIndex >= numTravelers) return;

    nodeQueues[node].occupiedBy = wt.travelerIndex;
    nodeQueues[node].collecting = 0;
    nodeQueues[node].collectionStartTime = 0.0;

    Traveler *t = &travelers[wt.travelerIndex];
    if (t->controlPipe[1] >= 0) {
        sendEnterApproval(t->controlPipe[1], node);
    }
}

static void dispatchReadyNodes(NodeQueue *nodeQueues,
                               int numVertices,
                               Traveler *travelers,
                               int numTravelers,
                               SchedulerType scheduler,
                               double now) {
    for (int node = 0; node < numVertices; node++) {
        if (nodeQueues[node].occupiedBy != -1) {
            continue;
        }

        if (!nodeQueues[node].collecting) {
            continue;
        }

        if (nodeQueues[node].queueSize <= 0) {
            nodeQueues[node].collecting = 0;
            nodeQueues[node].collectionStartTime = 0.0;
            continue;
        }

        if ((now - nodeQueues[node].collectionStartTime) >= COLLECTION_WINDOW_SECONDS) {
            tryDispatchNextTraveler(nodeQueues, travelers, numTravelers, node, scheduler);
        }
    }
}

static void sleepSeconds(double seconds) {
    if (seconds <= 0.0) return;

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);

    if (ts.tv_nsec < 0) ts.tv_nsec = 0;
    if (ts.tv_nsec >= 1000000000L) ts.tv_nsec = 999999999L;

    nanosleep(&ts, NULL);
}

static void resetTravelerState(Traveler *t) {
    t->pipeFd[0] = -1;
    t->pipeFd[1] = -1;
    t->controlPipe[0] = -1;
    t->controlPipe[1] = -1;
    t->pid = 0;

    t->currentNode = t->src;
    t->nextNode = -1;
    t->finished = 0;
    t->started = 0;
    t->arrived = 0;
    t->signaled = 0;

    t->drawFromNode = t->src;
    t->drawToNode = t->src;
    t->moveStartTime = 0.0;
    t->moving = 0;
    t->pausedProgress = 0.0;
    t->edgeDuration = 1.0;

    t->waitingForNode = 0;
    t->insideNode = 0;
    t->waitFromNode = -1;
}

static void cleanupTravelerProcess(Traveler *t) {
    if (t->pid > 0 && !t->signaled) {
        kill(t->pid, SIGTERM);
        waitpid(t->pid, NULL, 0);
        t->signaled = 1;
    }

    if (t->pipeFd[0] >= 0) {
        close(t->pipeFd[0]);
        t->pipeFd[0] = -1;
    }
    if (t->pipeFd[1] >= 0) {
        close(t->pipeFd[1]);
        t->pipeFd[1] = -1;
    }
    if (t->controlPipe[0] >= 0) {
        close(t->controlPipe[0]);
        t->controlPipe[0] = -1;
    }
    if (t->controlPipe[1] >= 0) {
        close(t->controlPipe[1]);
        t->controlPipe[1] = -1;
    }

    t->pid = 0;
}

static void reapFinishedTraveler(Traveler *t) {
    if (t->pid > 0 && t->finished && !t->signaled) {
        waitpid(t->pid, NULL, 0);
        t->signaled = 1;
        t->pid = 0;
    }
}

static void getEdgeEndpoints(VisGraph *vg, int from, int to,
                             Vector2 *start, Vector2 *end) {
    Vector2 p1 = vg->pos[from];
    Vector2 p2 = vg->pos[to];
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.001f) {
        *start = p1;
        *end = p2;
        return;
    }

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

static int loadVisGraph(const char *path, VisGraph *vg) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    int N, M;
    skipCommentsWS(fp);
    if (fscanf(fp, "%d %d", &N, &M) != 2) {
        fclose(fp);
        return 0;
    }

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

static int getEdgeWeight(const VisGraph *vg, int from, int to) {
    for (int i = 0; i < vg->numEdges; i++) {
        if (vg->edges[i].from == from && vg->edges[i].to == to) {
            return vg->edges[i].weight;
        }
    }
    return 1;
}

static int getPathEdgeWeight(Graph *g, int from, int to) {
    for (Edge *e = g->adj[from]; e != NULL; e = e->next) {
        if (e->to == to) {
            return e->weight;
        }
    }
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

    int isSrc = 0, isDest = 0;
    int hasInsideTraveler = 0;

    for (int t = 0; t < numTravelers; t++) {
        if (travelers[t].src == i)  isSrc = 1;
        if (travelers[t].dest == i) isDest = 1;
        if (travelers[t].insideNode && travelers[t].currentNode == i) {
            hasInsideTraveler = 1;
        }
    }

    if (hasInsideTraveler)           fill = (Color){255, 170, 40, 255};
    else if (isSrc && isDest)        fill = (Color){180, 80, 200, 255};
    else if (isSrc)                  fill = (Color){40, 180, 100, 255};
    else if (isDest)                 fill = (Color){220, 80, 80, 255};

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

static Color travelerColor(int idx) {
    static const Color palette[] = {
        {255, 220,  60, 255},
        {  0, 200, 255, 255},
        {255, 120, 200, 255},
        {120, 255, 120, 255},
        {255, 140,  40, 255},
        {200, 120, 255, 255},
        {255,  80,  80, 255},
        { 80, 200, 160, 255},
        {220, 220, 220, 255},
        {255, 200, 150, 255},
        {150, 200, 255, 255},
        {200, 255,  80, 255},
        {255, 100, 150, 255},
        {100, 255, 220, 255},
        {180, 180, 255, 255},
        {255, 180,  80, 255},
    };

    int n = (int)(sizeof(palette) / sizeof(palette[0]));
    return palette[idx % n];
}

static void processTravelerMessages(Traveler *travelers,
                                    int numTravelers,
                                    const VisGraph *vg,
                                    NodeQueue *nodeQueues,
                                    SchedulerType scheduler,
                                    long long *arrivalCounter,
                                    double now) {
    (void)scheduler;

    for (int i = 0; i < numTravelers; i++) {
        TravelerMessage msg;

        if (travelers[i].pipeFd[0] < 0) {
            continue;
        }

        while (1) {
            ssize_t n = read(travelers[i].pipeFd[0], &msg, sizeof(msg));

            if (n != (ssize_t)sizeof(msg)) {
                break;
            }

            if (msg.travelerIndex < 0 || msg.travelerIndex >= numTravelers) {
                continue;
            }

            Traveler *t = &travelers[msg.travelerIndex];
            t->currentNode = msg.currentNode;
            t->nextNode = msg.nextNode;

            if (msg.type == MSG_REQUEST_NODE) {
                t->waitingForNode = 1;
                t->insideNode = 0;
                t->moving = 0;
                t->drawFromNode = msg.currentNode;
                t->drawToNode = msg.currentNode;

                printf("[PID=%d] request node %d | next node: %d | edge weight: %d\n",
                       (int)msg.pid, msg.currentNode, msg.nextNode, msg.nextEdgeWeight);

                if (msg.currentNode >= 0 && msg.currentNode < MAX_VERTICES) {
                    WaitingTraveler wt;
                    wt.travelerIndex = msg.travelerIndex;
                    wt.pid = msg.pid;
                    wt.currentNode = msg.currentNode;
                    wt.nextNode = msg.nextNode;
                    wt.nextEdgeWeight = msg.nextEdgeWeight;
                    wt.arrivalOrder = (*arrivalCounter)++;

                    enqueueTraveler(nodeQueues, &wt);

                    if (nodeQueues[msg.currentNode].occupiedBy == -1) {
                        startCollectionWindow(nodeQueues, msg.currentNode, now);
                    }
                }
            }
            else if (msg.type == MSG_ENTERED_NODE) {
                t->waitingForNode = 0;
                t->insideNode = 1;
                t->moving = 0;
                t->drawFromNode = msg.currentNode;
                t->drawToNode = msg.currentNode;
                t->pausedProgress = 0.0;

                if (msg.isDestination) {
                    printf("[PID=%d] entered node %d | DESTINATION\n",
                           (int)msg.pid, msg.currentNode);
                } else {
                    printf("[PID=%d] entered node %d | next node: %d\n",
                           (int)msg.pid, msg.currentNode, msg.nextNode);
                }
            }
            else if (msg.type == MSG_LEAVING_NODE) {
                int weight = getEdgeWeight(vg, msg.currentNode, msg.nextNode);
                if (weight <= 0) weight = 1;

                t->waitingForNode = 0;
                t->insideNode = 0;
                t->drawFromNode = msg.currentNode;
                t->drawToNode = msg.nextNode;
                t->moveStartTime = GetTime();
                t->moving = 1;
                t->pausedProgress = 0.0;
                t->waitFromNode = msg.currentNode;
                t->edgeDuration = weight * SECONDS_PER_WEIGHT;
                if (t->edgeDuration < MIN_EDGE_DURATION) {
                    t->edgeDuration = MIN_EDGE_DURATION;
                }

                if (msg.currentNode >= 0 && msg.currentNode < MAX_VERTICES) {
                    if (nodeQueues[msg.currentNode].occupiedBy == msg.travelerIndex) {
                        nodeQueues[msg.currentNode].occupiedBy = -1;
                    }
                    if (nodeQueues[msg.currentNode].queueSize > 0) {
                        startCollectionWindow(nodeQueues, msg.currentNode, now);
                    }
                }

                printf("[PID=%d] leaving node %d | moving to node %d\n",
                       (int)msg.pid, msg.currentNode, msg.nextNode);
            }
            else if (msg.type == MSG_FINISHED) {
                t->finished = 1;
                t->waitingForNode = 0;
                t->insideNode = 0;
                t->moving = 0;
                t->pausedProgress = 0.0;
                t->arrived = 1;

                if (msg.currentNode >= 0 && msg.currentNode < MAX_VERTICES) {
                    if (nodeQueues[msg.currentNode].occupiedBy == msg.travelerIndex) {
                        nodeQueues[msg.currentNode].occupiedBy = -1;
                    }
                    if (nodeQueues[msg.currentNode].queueSize > 0) {
                        startCollectionWindow(nodeQueues, msg.currentNode, now);
                    }
                }

                printf("[PID=%d] finished\n", (int)msg.pid);
            }

            fflush(stdout);
        }
    }
}

static void startHandler(int sig) {
    (void)sig;
}

static void childMain(const char *filename, int travelerIndex,
                      int src, int dest, int writeFd, int approvalFd) {
    FILE *fp = NULL;
    Graph *g;

    signal(SIGUSR1, startHandler);
    pause();

    g = loadGraphOnly(filename, &fp);
    if (fp != NULL) {
        fclose(fp);
    }

    if (g == NULL) {
        close(writeFd);
        close(approvalFd);
        exit(1);
    }

    DijkstraResult result = dijkstra(g, src, dest);

    if (!result.found || result.pathLength <= 0) {
        TravelerMessage msg = {
            .type = MSG_FINISHED,
            .pid = getpid(),
            .travelerIndex = travelerIndex,
            .currentNode = dest,
            .nextNode = -1,
            .nextEdgeWeight = 0,
            .isDestination = 0
        };
        sendTravelerMessage(writeFd, &msg);

        freeDijkstraResult(&result);
        freeGraph(g);
        close(writeFd);
        close(approvalFd);
        exit(0);
    }

    for (int i = 0; i < result.pathLength; i++) {
        int current = result.path[i];
        int next = (i < result.pathLength - 1) ? result.path[i + 1] : -1;
        int isDest = (i == result.pathLength - 1);
        int nextWeight = 0;

        if (!isDest) {
            nextWeight = getPathEdgeWeight(g, current, next);
            if (nextWeight <= 0) nextWeight = 1;
        }

        TravelerMessage req = {
            .type = MSG_REQUEST_NODE,
            .pid = getpid(),
            .travelerIndex = travelerIndex,
            .currentNode = current,
            .nextNode = next,
            .nextEdgeWeight = nextWeight,
            .isDestination = isDest
        };
        sendTravelerMessage(writeFd, &req);

        if (!waitForEnterApproval(approvalFd, current)) {
            freeDijkstraResult(&result);
            freeGraph(g);
            close(writeFd);
            close(approvalFd);
            exit(1);
        }

        TravelerMessage entered = {
            .type = MSG_ENTERED_NODE,
            .pid = getpid(),
            .travelerIndex = travelerIndex,
            .currentNode = current,
            .nextNode = next,
            .nextEdgeWeight = nextWeight,
            .isDestination = isDest
        };
        sendTravelerMessage(writeFd, &entered);

        sleepSeconds(NODE_STAY_SECONDS);

        if (!isDest) {
            TravelerMessage leaving = {
                .type = MSG_LEAVING_NODE,
                .pid = getpid(),
                .travelerIndex = travelerIndex,
                .currentNode = current,
                .nextNode = next,
                .nextEdgeWeight = nextWeight,
                .isDestination = 0
            };
            sendTravelerMessage(writeFd, &leaving);

            double duration = nextWeight * SECONDS_PER_WEIGHT;
            if (duration < MIN_EDGE_DURATION) duration = MIN_EDGE_DURATION;
            sleepSeconds(duration);
        }
    }

    TravelerMessage finished = {
        .type = MSG_FINISHED,
        .pid = getpid(),
        .travelerIndex = travelerIndex,
        .currentNode = dest,
        .nextNode = -1,
        .nextEdgeWeight = 0,
        .isDestination = 1
    };
    sendTravelerMessage(writeFd, &finished);

    freeDijkstraResult(&result);
    freeGraph(g);
    close(writeFd);
    close(approvalFd);
    exit(0);
}

static int spawnTravelers(Traveler *travelers, int numTravelers, const char *filename) {
    for (int i = 0; i < numTravelers; i++) {
        resetTravelerState(&travelers[i]);

        if (pipe(travelers[i].pipeFd) < 0) {
            perror("pipe");
            return 0;
        }

        if (pipe(travelers[i].controlPipe) < 0) {
            perror("pipe");
            return 0;
        }
    }

    fflush(stdout);

    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 0;
        }

        if (pid == 0) {
            close(travelers[i].pipeFd[0]);
            close(travelers[i].controlPipe[1]);

            childMain(filename,
                      i,
                      travelers[i].src,
                      travelers[i].dest,
                      travelers[i].pipeFd[1],
                      travelers[i].controlPipe[0]);
        }

        travelers[i].pid = pid;

        close(travelers[i].pipeFd[1]);
        travelers[i].pipeFd[1] = -1;

        close(travelers[i].controlPipe[0]);
        travelers[i].controlPipe[0] = -1;

        int flags = fcntl(travelers[i].pipeFd[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(travelers[i].pipeFd[0], F_SETFL, flags | O_NONBLOCK);
        }
    }

    return 1;
}

void runGraphVisualizer(const char *filename, SchedulerType scheduler) {
    const int W = 900;
    const int H = 700;
    int isPaused = 0;
    long long arrivalCounter = 0;

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

    NodeQueue nodeQueues[MAX_VERTICES];
    initNodeQueues(nodeQueues, algoGraph->numVertices);

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

        if (s < 0 || s >= algoGraph->numVertices ||
            d < 0 || d >= algoGraph->numVertices) {
            printf("Traveler %d out of range\n", i + 1);
            fclose(fp);
            free(travelers);
            freeGraph(algoGraph);
            return;
        }

        travelers[i].src = s;
        travelers[i].dest = d;
        travelers[i].color = travelerColor(i);
        resetTravelerState(&travelers[i]);
    }
    fclose(fp);

    for (int i = 0; i < numTravelers; i++) {
        travelers[i].result = dijkstra(algoGraph,
                                       travelers[i].src,
                                       travelers[i].dest);
    }

    if (!spawnTravelers(travelers, numTravelers, filename)) {
        for (int i = 0; i < numTravelers; i++) {
            freeDijkstraResult(&travelers[i].result);
        }
        free(travelers);
        freeGraph(algoGraph);
        return;
    }

    VisGraph vg = {0};
    if (!loadVisGraph(filename, &vg)) {
        for (int i = 0; i < numTravelers; i++) {
            cleanupTravelerProcess(&travelers[i]);
            freeDijkstraResult(&travelers[i].result);
        }
        free(travelers);
        freeGraph(algoGraph);
        return;
    }

    InitWindow(W, H, "Graph Visualizer - Milestone 7");
    SetTargetFPS(60);
    computeLayout(&vg, W, H);
    Font font = GetFontDefault();

    while (!WindowShouldClose()) {
        double now = GetTime();

        processTravelerMessages(travelers,
                                numTravelers,
                                &vg,
                                nodeQueues,
                                scheduler,
                                &arrivalCounter,
                                now);

        dispatchReadyNodes(nodeQueues,
                           algoGraph->numVertices,
                           travelers,
                           numTravelers,
                           scheduler,
                           now);

        for (int i = 0; i < numTravelers; i++) {
            reapFinishedTraveler(&travelers[i]);
        }

        BeginDrawing();
        ClearBackground(BG_COLOR);

        for (int x = 0; x < W; x += 40) {
            for (int y = 0; y < H; y += 40) {
                DrawPixel(x, y, (Color){60, 70, 100, 80});
            }
        }

        DrawRectangle(0, 0, W, TOP_BAR_HEIGHT, (Color){20, 24, 36, 255});
        DrawLine(0, TOP_BAR_HEIGHT, W, TOP_BAR_HEIGHT, (Color){60, 70, 100, 180});

        char title[128];
        snprintf(title, sizeof(title),
                 "Graph Visualizer - Milestone 7 (%s)",
                 schedulerName(scheduler));

        Vector2 ts = MeasureTextEx(font, title, 26, 1);
        DrawTextEx(font,
                   title,
                   (Vector2){24, (TOP_BAR_HEIGHT - ts.y) / 2.0f},
                   26, 1, TITLE_COLOR);

        Rectangle resetButton = {
            W - 24 - BUTTON_W,
            (TOP_BAR_HEIGHT - BUTTON_H) / 2.0f,
            BUTTON_W,
            BUTTON_H
        };

        Rectangle playButton = {
            resetButton.x - BUTTON_GAP - BUTTON_W,
            (TOP_BAR_HEIGHT - BUTTON_H) / 2.0f,
            BUTTON_W,
            BUTTON_H
        };

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();

            if (CheckCollisionPointRec(mouse, playButton)) {
                int anyStarted = 0;
                for (int i = 0; i < numTravelers; i++) {
                    if (travelers[i].started && !travelers[i].finished) {
                        anyStarted = 1;
                        break;
                    }
                }

                if (!anyStarted) {
                    for (int i = 0; i < numTravelers; i++) {
                        Traveler *t = &travelers[i];
                        if (!t->started && t->pid > 0) {
                            kill(t->pid, SIGUSR1);
                            t->started = 1;
                        }
                    }
                    isPaused = 0;
                } else if (!isPaused) {
                    for (int i = 0; i < numTravelers; i++) {
                        Traveler *t = &travelers[i];
                        if (t->started && !t->finished && t->pid > 0) {
                            kill(t->pid, SIGSTOP);
                        }
                        if (t->moving) {
                            double elapsed = GetTime() - t->moveStartTime;
                            double duration = t->edgeDuration;
                            if (duration <= 0.0) duration = 1.0;
                            if (elapsed < 0.0) elapsed = 0.0;
                            if (elapsed > duration) elapsed = duration;
                            t->pausedProgress = elapsed;
                        }
                    }
                    isPaused = 1;
                } else {
                    for (int i = 0; i < numTravelers; i++) {
                        Traveler *t = &travelers[i];
                        if (t->started && !t->finished && t->pid > 0) {
                            kill(t->pid, SIGCONT);
                        }
                        if (t->moving) {
                            t->moveStartTime = GetTime() - t->pausedProgress;
                        }
                    }
                    isPaused = 0;
                }
            }

            if (CheckCollisionPointRec(mouse, resetButton)) {
                for (int i = 0; i < numTravelers; i++) {
                    cleanupTravelerProcess(&travelers[i]);
                    resetTravelerState(&travelers[i]);
                }

                if (!spawnTravelers(travelers, numTravelers, filename)) {
                    isPaused = 0;
                }

                initNodeQueues(nodeQueues, algoGraph->numVertices);
                arrivalCounter = 0;
                isPaused = 0;
            }
        }

        const char *playLabel = "Play";
        int anyStarted = 0;
        for (int i = 0; i < numTravelers; i++) {
            if (travelers[i].started && !travelers[i].finished) {
                anyStarted = 1;
                break;
            }
        }
        if (anyStarted) {
            playLabel = isPaused ? "Resume" : "Pause";
        }

        DrawRectangleRounded(playButton, 0.25f, 8,
                             isPaused ? (Color){0, 120, 180, 255}
                                      : (Color){0, 140, 40, 255});
        Vector2 playTextSize = MeasureTextEx(font, playLabel, 20, 1);
        DrawTextEx(font,
                   playLabel,
                   (Vector2){
                       playButton.x + (playButton.width - playTextSize.x) / 2.0f,
                       playButton.y + (playButton.height - playTextSize.y) / 2.0f
                   },
                   20, 1, WHITE);

        DrawRectangleRounded(resetButton, 0.25f, 8, (Color){210, 30, 70, 255});
        Vector2 resetTextSize = MeasureTextEx(font, "Reset", 20, 1);
        DrawTextEx(font,
                   "Reset",
                   (Vector2){
                       resetButton.x + (resetButton.width - resetTextSize.x) / 2.0f,
                       resetButton.y + (resetButton.height - resetTextSize.y) / 2.0f
                   },
                   20, 1, WHITE);

        for (int i = 0; i < vg.numEdges; i++) drawEdge(&vg, i, font);
        for (int i = 0; i < vg.numVertices; i++) drawNode(&vg, i, font, travelers, numTravelers);

        for (int i = 0; i < numTravelers; i++) {
            Traveler *t = &travelers[i];

            if (t->drawFromNode < 0 || t->drawFromNode >= vg.numVertices) {
                continue;
            }

            Vector2 entityPos;
            Vector2 basePos;

            if (t->waitingForNode && t->waitFromNode >= 0 &&
                t->currentNode >= 0 && t->currentNode < vg.numVertices &&
                t->waitFromNode < vg.numVertices) {

                Vector2 start, end;
                getEdgeEndpoints(&vg, t->waitFromNode, t->currentNode, &start, &end);

                float dx = end.x - start.x;
                float dy = end.y - start.y;
                float len = sqrtf(dx * dx + dy * dy);

                if (len > 0.001f) {
                    float ux = dx / len;
                    float uy = dy / len;
                    float waitDistance = 22.0f;

                    entityPos.x = end.x - ux * waitDistance;
                    entityPos.y = end.y - uy * waitDistance;
                } else {
                    entityPos = vg.pos[t->currentNode];
                }

                basePos = entityPos;
            }
            else if (t->moving && t->drawToNode >= 0 && t->drawToNode < vg.numVertices) {
                Vector2 start, end;
                getEdgeEndpoints(&vg, t->drawFromNode, t->drawToNode, &start, &end);

                double elapsed = isPaused ? t->pausedProgress : (GetTime() - t->moveStartTime);
                double duration = t->edgeDuration;
                if (duration <= 0.0) duration = 1.0;

                float progress = (float)(elapsed / duration);

                if (progress > 1.0f) progress = 1.0f;
                if (progress < 0.0f) progress = 0.0f;

                entityPos.x = start.x + (end.x - start.x) * progress;
                entityPos.y = start.y + (end.y - start.y) * progress;
                basePos = entityPos;
            } else {
                basePos = vg.pos[t->drawFromNode];
                entityPos = basePos;
            }

            Color travelerDrawColor = t->color;
            if (t->waitingForNode) {
                travelerDrawColor = ORANGE;
            } else if (t->insideNode) {
                travelerDrawColor = GREEN;
            }

            DrawCircle((int)entityPos.x, (int)entityPos.y, 16,
                       (Color){travelerDrawColor.r, travelerDrawColor.g, travelerDrawColor.b, 100});
            DrawCircle((int)entityPos.x, (int)entityPos.y, 11, travelerDrawColor);
            DrawCircleLines((int)entityPos.x, (int)entityPos.y, 11, WHITE);

            char lab[16];
            sprintf(lab, "T%d", i + 1);
            DrawTextEx(font, lab,
                       (Vector2){entityPos.x + 14, entityPos.y - 8},
                       14, 1, WHITE);
        }

        int panelX = 20;
        int panelY = TOP_BAR_HEIGHT + 20;

        char schedLine[64];
        snprintf(schedLine, sizeof(schedLine), "Scheduler: %s", schedulerName(scheduler));
        DrawTextEx(font, schedLine, (Vector2){panelX, panelY}, 16, 1, YELLOW);

        DrawTextEx(font, "Travelers:",
                   (Vector2){panelX, panelY + 24}, 16, 1, WHITE);

        for (int i = 0; i < numTravelers; i++) {
            Traveler *t = &travelers[i];
            int yy = panelY + 48 + i * 22;

            Color panelColor = t->color;
            if (t->waitingForNode) panelColor = ORANGE;
            else if (t->insideNode) panelColor = GREEN;

            DrawCircle(panelX + 8, yy + 8, 7, panelColor);

            const char *status;
            if (t->finished) status = "[finished]";
            else if (isPaused && t->started && !t->finished) status = "[paused]";
            else if (t->waitingForNode) status = "[waiting node]";
            else if (t->insideNode) status = "[inside node]";
            else if (t->moving) status = "[moving]";
            else if (t->started) status = "[running]";
            else status = "[waiting]";

            char line[140];
            snprintf(line, sizeof(line), "T%d  %d -> %d  at:%d  next:%d %s",
                     i + 1,
                     t->src,
                     t->dest,
                     t->currentNode,
                     t->nextNode,
                     status);

            DrawTextEx(font, line, (Vector2){panelX + 22, yy}, 14, 1, WHITE);
        }

        int allDone = 1;
        for (int i = 0; i < numTravelers; i++) {
            if (!travelers[i].finished) {
                allDone = 0;
                break;
            }
        }

        if (allDone) {
            DrawTextEx(font, "All travelers finished!",
                       (Vector2){W / 2.0f - 120, H - 40},
                       22, 1, YELLOW);
        }

        EndDrawing();
    }

    CloseWindow();

    for (int i = 0; i < numTravelers; i++) {
        cleanupTravelerProcess(&travelers[i]);
        freeDijkstraResult(&travelers[i].result);
    }

    free(travelers);
    freeGraph(algoGraph);
}
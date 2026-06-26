/* ============================================================================
 *  GraphVisual.c  —  Multi-traveler shortest-path simulator (Milestones 4-7)
 * ============================================================================
 *
 *  BIG PICTURE
 *  -----------
 *  This file runs ONE parent process (the GUI) plus ONE child process per
 *  "traveler". Each traveler walks its Dijkstra shortest path through the
 *  graph. The parent draws everything and acts as a traffic controller that
 *  decides which traveler is allowed to occupy a node at any moment.
 *
 *  PROCESS MODEL (this is the part exam tasks usually touch)
 *  ---------------------------------------------------------
 *    parent (runGraphVisualizer)
 *      |
 *      |-- fork() --> child 0  (childMain)   \
 *      |-- fork() --> child 1  (childMain)    > one child per traveler
 *      |-- fork() --> child 2  (childMain)   /
 *
 *  COMMUNICATION: two pipes per traveler
 *      data pipe     child -> parent : child reports "I want node X / I
 *                                       entered / I'm leaving / I finished"
 *                                       (TravelerMessage structs)
 *      control pipe  parent -> child : parent sends "approved, enter node X"
 *                                       (a single int)
 *
 *  SIGNALS used between parent and child:
 *      SIGUSR1  parent -> child : "Play pressed, start moving" (wakes pause())
 *      SIGSTOP  parent -> child : freeze the child   (Pause button)
 *      SIGCONT  parent -> child : unfreeze the child (Resume button)
 *      SIGTERM  parent -> child : kill the child immediately (Reset / quit)
 *               ^^^^^^ this is "the signal that kills it" in the sample task.
 *
 *  CHILD LIFECYCLE (childMain): install SIGUSR1 handler -> pause() until Play
 *      -> run Dijkstra -> for each node: request, wait approval, enter, sleep,
 *      leave (sleep across edge) -> send FINISHED -> exit(0).
 *
 *  All sleeping goes through sleepSeconds() -> nanosleep().
 * ============================================================================ */

/* Enable POSIX APIs (kill, waitpid, SIGTERM, pause) under -std=c11.
 * Without this define the compiler hides fork/kill/pause/etc. in strict C11. */
#define _POSIX_C_SOURCE 200809L

#include "raylib.h"        /* GUI: window, drawing, input, timing (GetTime) */
#include "Dijkstra.h"      /* Graph type, dijkstra(), loadGraphOnly(), etc.   */
#include <stdio.h>         /* printf, fscanf, FILE                            */
#include <stdlib.h>        /* malloc/calloc/free, exit                        */
#include <math.h>          /* sqrtf, cosf, sinf for geometry                  */
#include <string.h>        /* (string helpers)                                */
#include <unistd.h>        /* fork, pipe, read, write, close, pause           */
#include <sys/types.h>     /* pid_t                                           */
#include <sys/wait.h>      /* waitpid                                         */
#include <signal.h>        /* kill, signal, SIGUSR1/SIGTERM/SIGSTOP/SIGCONT   */
#include <errno.h>         /* errno                                           */
#include <ctype.h>         /* isspace                                         */
#include <fcntl.h>         /* fcntl + O_NONBLOCK (non-blocking pipe reads)    */
#include <time.h>          /* struct timespec, nanosleep                      */

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

/* ===== visual + simulation constants ===== */
#define MAX_VERTICES   15        /* hard cap on graph nodes (fixed arrays)      */
#define MAX_TRAVELERS  16        /* hard cap on travelers / child processes     */
#define NODE_RADIUS    28        /* drawn circle radius for a node, in pixels   */
#define ARROW_HEAD     12        /* length of an edge arrow head                */
#define ARROW_ANGLE    0.42f     /* half-angle of the arrow head triangle       */
#define EDGE_OFFSET    6.0f      /* sideways shift so A->B and B->A don't overlap*/

/* Color palette (raylib Color = R,G,B,A bytes). Pure cosmetics. */
#define BG_COLOR       (Color){15, 17, 26, 255}
#define NODE_COLOR     (Color){40, 120, 220, 255}
#define NODE_OUTLINE   (Color){100, 180, 255, 255}
#define NODE_TEXT      WHITE
#define EDGE_COLOR     (Color){180, 190, 210, 200}
#define ARROW_COLOR    (Color){220, 230, 255, 230}
#define WEIGHT_BG      (Color){30, 35, 50, 210}
#define WEIGHT_TEXT    (Color){255, 220, 80, 255}
#define TITLE_COLOR    (Color){100, 180, 255, 255}

#define TOP_BAR_HEIGHT 80        /* height of the top toolbar (Play/Reset)      */
#define BUTTON_W       120
#define BUTTON_H       40
#define BUTTON_GAP     16

/* edge movement timing: how long a traveler takes to cross an edge.
 * Travel time = weight * SECONDS_PER_WEIGHT, but never below MIN_EDGE_DURATION. */
#define SECONDS_PER_WEIGHT    0.4
#define MIN_EDGE_DURATION     0.4

/* milestone 6/7 timing */
#define NODE_STAY_SECONDS       1.0   /* how long a traveler sits inside a node  */
#define COLLECTION_WINDOW_SECONDS 0.20/* grace period to gather rival travelers
                                       * competing for the same free node before
                                       * the scheduler picks a winner            */

/* Which scheduling policy decides who enters a contested node first. */
typedef enum {
    SCHED_FCFS,
    SCHED_SJF,
    SCHED_PRIORITY
} SchedulerType;

/* One directed edge exactly as read from the graph file (for DRAWING only). */
typedef struct RawEdge {
    int from;     /* source vertex index      */
    int to;       /* destination vertex index */
    int weight;   /* edge cost / travel weight*/
} RawEdge;

/* The "visual" graph: the raw edge list plus on-screen positions of nodes.
 * This is separate from the algorithmic Graph (adjacency list) used by Dijkstra. */
typedef struct {
    int numVertices;                        /* node count                      */
    int numEdges;                           /* edge count                      */
    RawEdge edges[MAX_VERTICES * MAX_VERTICES];/* every edge, for drawing       */
    Vector2 pos[MAX_VERTICES];              /* pixel (x,y) of each node         */
} VisGraph;

/* The kinds of status messages a child sends to the parent over the data pipe. */
typedef enum {
    MSG_REQUEST_NODE,   /* "I want to enter node X" (asks permission)          */
    MSG_ENTERED_NODE,   /* "I am now inside node X"                            */
    MSG_LEAVING_NODE,   /* "I am leaving node X toward node Y"                 */
    MSG_FINISHED        /* "I reached my destination / I'm done"               */
} MessageType;

/* The actual packet written through the pipe. A whole struct is written/read
 * at once with write()/read(), so both sides agree on the exact byte layout. */
typedef struct {
    MessageType type;        /* which event this message describes              */
    pid_t pid;               /* child's process id (used for the printf logs)  */
    int travelerIndex;       /* which traveler (0..numTravelers-1) sent it     */
    int currentNode;         /* node the traveler is at / talking about        */
    int nextNode;            /* node it intends to move to next (-1 if none)    */
    int nextEdgeWeight;      /* weight of currentNode->nextNode (for SJF)       */
    int isDestination;       /* 1 if currentNode is this traveler's final dest  */
} TravelerMessage;

/* A traveler currently WAITING in a node's queue for permission to enter it. */
typedef struct {
    int travelerIndex;       /* who is waiting                                  */
    pid_t pid;               /* its pid (logging)                               */
    int currentNode;         /* the node it wants to enter                      */
    int nextNode;            /* where it will go after (for SJF tie-breaking)   */
    int nextEdgeWeight;      /* weight used by the SJF policy                    */
    long long arrivalOrder;  /* global counter: who asked first (for FCFS)      */
} WaitingTraveler;

/* Per-node "gatekeeper" state: who is inside, and who is lined up waiting. */
typedef struct {
    int occupiedBy;                    /* traveler index inside, or -1 if free  */
    WaitingTraveler queue[MAX_TRAVELERS];/* travelers waiting for this node     */
    int queueSize;                     /* how many are waiting                  */

    int collecting;                    /* 1 while the collection window is open */
    double collectionStartTime;        /* when that window started (GetTime)    */
} NodeQueue;

/* The parent's full record for ONE traveler: its plan, its child process,
 * its pipes, and all the bookkeeping needed to animate and schedule it.
 * Grouped below into: plan / process / pipes / logical state / animation. */
typedef struct {
    /* --- the plan --- */
    int src;                 /* start vertex                                    */
    int dest;                /* goal vertex                                     */
    DijkstraResult result;   /* the computed shortest path (path[], length...)  */

    /* --- the child process --- */
    pid_t pid;               /* child's pid; 0 means "no live child"            */
    Color color;             /* this traveler's dot color in the GUI            */

    int arrived;             /* 1 once it has reached its destination           */
    int signaled;            /* 1 once we've already kill()+waitpid()'d it,
                              * so we don't try to reap/kill it twice           */

    /* --- the two pipes connecting parent and this child --- */
    int pipeFd[2];           /* data pipe   child -> parent ([0]=read,[1]=write)*/
    int controlPipe[2];      /* control pipe parent -> child ([0]=read,[1]=write)*/

    /* --- logical position / status (updated from child messages) --- */
    int currentNode;         /* node it is at right now                         */
    int nextNode;            /* node it is heading to                           */
    int finished;            /* 1 when MSG_FINISHED was received                */
    int started;             /* 1 after Play sent it the SIGUSR1 start signal   */

    /* --- animation state (purely for smooth on-screen movement) --- */
    int drawFromNode;        /* edge endpoint the dot is moving FROM            */
    int drawToNode;          /* edge endpoint the dot is moving TO              */
    double moveStartTime;    /* GetTime() when this edge crossing began         */
    int moving;              /* 1 while the dot is sliding along an edge         */
    double pausedProgress;   /* saved progress (seconds) while paused           */
    double edgeDuration;     /* total seconds this edge crossing should take    */

    /* --- finer status flags used for color + placement --- */
    int waitingForNode;      /* 1 while queued, waiting for entry permission    */
    int insideNode;          /* 1 while sitting inside a node                   */
    int waitFromNode;        /* the node it is waiting just outside of          */
} Traveler;

/* ===== helpers ===== */

/* Convert the scheduler enum to the text shown in the GUI side panel and title. */
static const char *schedulerName(SchedulerType scheduler) {
    switch (scheduler) {
        case SCHED_FCFS:
            return "FCFS";
        case SCHED_SJF:
            return "SJF";
        case SCHED_PRIORITY:
            return "PRIORITY";
        default:
            return "UNKNOWN";
    }
}

/* Reset every node queue so no traveler is occupying or waiting at startup/reset. */
static void initNodeQueues(NodeQueue *nodeQueues, int numVertices) {
    for (int i = 0; i < numVertices; i++) {
        nodeQueues[i].occupiedBy = -1;
        nodeQueues[i].queueSize = 0;
        nodeQueues[i].collecting = 0;
        nodeQueues[i].collectionStartTime = 0.0;
    }
}

/* Append one waiting traveler record to the queue of the requested node. */
static void enqueueTraveler(NodeQueue *nodeQueues, const WaitingTraveler *wt) {
    int node = wt->currentNode;

    if (node < 0 || node >= MAX_VERTICES) return;
    if (nodeQueues[node].queueSize >= MAX_TRAVELERS) return;

    nodeQueues[node].queue[nodeQueues[node].queueSize] = *wt;
    nodeQueues[node].queueSize++;
}

/* Choose the next traveler index to dispatch according to FCFS or SJF policy.
 * Returns an index INTO nodeQueues[node].queue (not a traveler index), or -1.
 * FCFS: pick the smallest arrivalOrder (whoever asked earliest).
 * SJF : pick the smallest nextEdgeWeight; ties broken by earliest arrival. */
static int pickNextTravelerIndex(const NodeQueue *nodeQueues, int node, SchedulerType scheduler) {
    if (node < 0 || node >= MAX_VERTICES) return -1;
    if (nodeQueues[node].queueSize <= 0) return -1;

    int best = 0;   /* assume the first waiter is best, then try to beat it */

    for (int i = 1; i < nodeQueues[node].queueSize; i++) {
        const WaitingTraveler *cand = &nodeQueues[node].queue[i];     /* candidate */
        const WaitingTraveler *cur  = &nodeQueues[node].queue[best];  /* current best */

        if (scheduler == SCHED_FCFS) {

            /* earlier arrival beats later arrival */
            if (cand->arrivalOrder < cur->arrivalOrder) {
                best = i;
            }

        }
        else if (scheduler == SCHED_SJF) {

            /* SJF: lighter next edge wins; equal weight -> earlier arrival wins */
            if (cand->nextEdgeWeight < cur->nextEdgeWeight) {
                best = i;
            }
            else if (cand->nextEdgeWeight == cur->nextEdgeWeight &&
                     cand->arrivalOrder < cur->arrivalOrder) {
                best = i;
                     }

        }
        else if (scheduler == SCHED_PRIORITY) {

            /* Priority: lowest PID wins */
            if (cand->pid < cur->pid) {
                best = i;
            }

        }
    }

    return best;
}

/* Remove one waiting traveler from a node queue and compact the remaining entries. */
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

/* === Pipe communication primitives (child <-> parent) ===
 * Each message is a whole fixed-size struct, written/read in one shot so the
 * two processes never disagree about where one message ends and the next begins. */

/* CHILD -> PARENT: write one status message into the data pipe.
 * Returns 1 on a full successful write, 0 otherwise. */
static int sendTravelerMessage(int fd, const TravelerMessage *msg) {
    ssize_t n = write(fd, msg, sizeof(*msg));
    return (n == (ssize_t)sizeof(*msg));
}

/* PARENT -> CHILD: write the approved node number into the control pipe.
 * This is how the parent says "you may now enter node X". */
static int sendEnterApproval(int fd, int node) {
    ssize_t n = write(fd, &node, sizeof(node));
    return (n == (ssize_t)sizeof(node));
}

/* CHILD side: block on read() until the parent sends an approval.
 * read() blocks the child here (the control pipe is NOT non-blocking),
 * which is exactly how a traveler "waits its turn" at a node.
 * Returns 1 only if the approved node equals the one we asked for. */
static int waitForEnterApproval(int fd, int expectedNode) {
    int approvedNode = -1;
    ssize_t n = read(fd, &approvedNode, sizeof(approvedNode));
    if (n != (ssize_t)sizeof(approvedNode)) {
        return 0;
    }
    return (approvedNode == expectedNode);
}

/* Start the short collection window that lets multiple contenders reach the same free node. */
static void startCollectionWindow(NodeQueue *nodeQueues, int node, double now) {
    if (node < 0 || node >= MAX_VERTICES) return;
    if (nodeQueues[node].occupiedBy != -1) return;
    if (nodeQueues[node].queueSize <= 0) return;
    if (nodeQueues[node].collecting) return;

    nodeQueues[node].collecting = 1;
    nodeQueues[node].collectionStartTime = now;
}

/* Dispatch the best waiting traveler for a node and send that traveler an approval message. */
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

/* Scan all nodes and dispatch any queue whose collection window has expired. */
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

/* Sleep for a floating-point number of seconds using nanosleep.
 * EVERY pause a child takes (sitting in a node, crossing an edge) goes through
 * here, so this is the single place where "how long it slept" is spent.
 * It splits the double into whole seconds (tv_sec) + nanoseconds (tv_nsec). */
static void sleepSeconds(double seconds) {
    if (seconds <= 0.0) return;

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;                              /* whole seconds   */
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9); /* leftover as ns  */

    /* clamp the nanosecond field into the legal range [0, 999999999] */
    if (ts.tv_nsec < 0) ts.tv_nsec = 0;
    if (ts.tv_nsec >= 1000000000L) ts.tv_nsec = 999999999L;

    nanosleep(&ts, NULL);
}

/* Reset one traveler's runtime and animation state before spawning or after reset. */
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

/* Stop a traveler process if needed and close every pipe file descriptor it owns.
 * THIS is the "forced death" path (Reset button, quit, error). It sends SIGTERM
 * — whose default action terminates the child instantly, with no chance to print
 * or clean up. The sample exam task asks you to change THIS behavior: send a
 * different signal (SIGUSR1) and let the child report + exit on its own.
 * `!t->signaled` guards against killing/reaping the same child twice. */
static void cleanupTravelerProcess(Traveler *t) {
    if (t->pid > 0 && !t->signaled) {
        kill(t->pid, SIGTERM);      /* <-- "the signal that kills it" */
        waitpid(t->pid, NULL, 0);   /* reap the zombie so it fully disappears */
        t->signaled = 1;
    }

    /* close all four pipe ends this traveler owns, guarding against double-close */
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

/* Reap a child that finished NATURALLY (it already sent MSG_FINISHED and called
 * exit(0) on its own). We only waitpid() to clear the zombie entry; we do not
 * signal it, because it is already exiting by itself. */
static void reapFinishedTraveler(Traveler *t) {
    if (t->pid > 0 && t->finished && !t->signaled) {
        waitpid(t->pid, NULL, 0);
        t->signaled = 1;
        t->pid = 0;
    }
}

/* ==========================================================================
 *  GEOMETRY & DRAWING HELPERS
 *  Pure presentation code: where to place nodes, how to draw edges, arrows,
 *  weight labels, and node circles. None of this touches processes or signals.
 * ========================================================================== */

/* Compute the start/end drawing points of an edge after applying node radius and edge offset.
 * Shifts each endpoint off the node's rim and sideways by EDGE_OFFSET so that
 * the A->B arrow and the B->A arrow are drawn as two distinct parallel lines. */
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

/* Place all graph vertices in a circular layout inside the simulation window. */
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

/* Load only the visual edge list used for drawing nodes, edges, and weights. */
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

/* Return the weight of a visual edge so animation timing can match the drawn graph. */
static int getEdgeWeight(const VisGraph *vg, int from, int to) {
    for (int i = 0; i < vg->numEdges; i++) {
        if (vg->edges[i].from == from && vg->edges[i].to == to) {
            return vg->edges[i].weight;
        }
    }
    return 1;
}

/* Return the weight of one edge along a traveler's shortest path in the adjacency list. */
static int getPathEdgeWeight(Graph *g, int from, int to) {
    for (Edge *e = g->adj[from]; e != NULL; e = e->next) {
        if (e->to == to) {
            return e->weight;
        }
    }
    return 1;
}

/* Draw a triangular arrow head that points in the direction of a line segment. */
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

/* Draw one graph edge, including self-loops, arrow heads, and weight labels. */
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

/* Draw one node and color it according to whether it is a source, destination, or occupied. */
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

/* Pick a stable color from the palette for traveler i. */
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

/* Read all pending child messages, update traveler state, and enqueue node requests.
 * Called once per frame by the parent. For each traveler it drains its data pipe
 * in a while-loop until read() returns no full message (the pipe was set
 * NON-BLOCKING in spawnTravelers, so read() returns immediately when empty
 * instead of freezing the GUI). Each message type drives a small state update:
 *   MSG_REQUEST_NODE -> put the traveler in that node's waiting queue
 *   MSG_ENTERED_NODE -> mark it inside the node
 *   MSG_LEAVING_NODE -> free the node, start the edge animation, wake the queue
 *   MSG_FINISHED     -> mark done, free the node
 * The printf lines are the milestone's required terminal log. */
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

/* ==========================================================================
 *  PROCESSES, SIGNALS & CHILD LIFECYCLE
 *  The core of the assignment: the signal handler, the child's main loop,
 *  and the fork() routine that spawns one child per traveler.
 *  >>> Sample exam tasks about signals almost always modify this section. <<<
 * ========================================================================== */

/* Minimal SIGUSR1 handler installed by each child.
 * Its ONLY job is to exist so that pause() returns when the parent sends SIGUSR1
 * (the "Play" signal). A handler that does nothing is enough to interrupt
 * pause(); without an installed handler, SIGUSR1's default action would kill the
 * child instead. NOTE for the exam: this is where you'd add logic if SIGUSR1
 * must also trigger a graceful "report sleep time and exit". */
static void startHandler(int sig) {
    (void)sig;   /* unused parameter; silence the compiler warning */
}

/* ============================ CHILD ENTRY POINT ============================
 * Runs in each forked child. It NEVER returns — it always ends in exit().
 * Parameters: graph file path, this traveler's index, src/dest vertices,
 *   writeFd     = our end of the data pipe   (child -> parent)
 *   approvalFd  = our end of the control pipe (parent -> child)
 *
 * Flow:
 *   1. install SIGUSR1 handler, then pause() -> freeze until Play
 *   2. load the graph and run Dijkstra to get our own path
 *   3. walk the path node by node: request -> wait approval -> enter ->
 *      sleep in node -> announce leaving -> sleep across edge
 *   4. send MSG_FINISHED and exit(0)
 * ========================================================================== */
static void childMain(const char *filename, int travelerIndex,
                      int src, int dest, int writeFd, int approvalFd) {
    FILE *fp = NULL;
    Graph *g;

    signal(SIGUSR1, startHandler); /* arm the start signal so pause() can be woken */
    pause();                       /* sleep here until parent sends SIGUSR1 (Play) */

    g = loadGraphOnly(filename, &fp);   /* each child loads its own copy of the graph */
    if (fp != NULL) {
        fclose(fp);
    }

    if (g == NULL) {                /* graph failed to load -> bail out */
        close(writeFd);
        close(approvalFd);
        exit(1);
    }

    DijkstraResult result = dijkstra(g, src, dest);  /* compute our shortest path */

    if (!result.found || result.pathLength <= 0) {   /* no path exists -> report done */
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

    /* Walk the shortest path one vertex at a time. */
    for (int i = 0; i < result.pathLength; i++) {
        int current = result.path[i];                               /* node we're at      */
        int next = (i < result.pathLength - 1) ? result.path[i + 1] : -1; /* node after    */
        int isDest = (i == result.pathLength - 1);                  /* last node?         */
        int nextWeight = 0;

        if (!isDest) {
            nextWeight = getPathEdgeWeight(g, current, next);        /* cost of next edge  */
            if (nextWeight <= 0) nextWeight = 1;
        }

        /* (1) ASK the parent for permission to enter `current`. */
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

        /* (2) BLOCK until the parent approves entry to this exact node. */
        if (!waitForEnterApproval(approvalFd, current)) {
            freeDijkstraResult(&result);
            freeGraph(g);
            close(writeFd);
            close(approvalFd);
            exit(1);
        }

        /* (3) Tell the parent we are now inside the node. */
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

        sleepSeconds(NODE_STAY_SECONDS);   /* <-- SLEEP #1: dwell time inside the node */

        if (!isDest) {
            /* (4) Announce we are leaving toward `next`. */
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

            /* (5) SLEEP #2: time spent crossing the edge, proportional to weight. */
            double duration = nextWeight * SECONDS_PER_WEIGHT;
            if (duration < MIN_EDGE_DURATION) duration = MIN_EDGE_DURATION;
            sleepSeconds(duration);
        }
    }

    /* Reached the destination: report FINISHED, free everything, exit cleanly. */
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

/* Create pipes and fork one child process for each traveler in the simulation.
 * Done in TWO passes: first create every pipe, then fork every child. Returns
 * 1 on success, 0 if any pipe()/fork() failed. */
static int spawnTravelers(Traveler *travelers, int numTravelers, const char *filename) {
    /* Pass 1: create both pipes for every traveler BEFORE any fork. */
    for (int i = 0; i < numTravelers; i++) {
        resetTravelerState(&travelers[i]);

        if (pipe(travelers[i].pipeFd) < 0) {       /* data pipe  child -> parent */
            perror("pipe");
            return 0;
        }

        if (pipe(travelers[i].controlPipe) < 0) {  /* control pipe parent -> child */
            perror("pipe");
            return 0;
        }
    }

    fflush(stdout);  /* flush before fork so buffered text isn't duplicated by children */

    /* Pass 2: fork one child per traveler. */
    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 0;
        }

        if (pid == 0) {
            /* ---- CHILD branch (fork returned 0) ---- */
            /* Close the ends the child does NOT use:
             *   - read end of its data pipe (child only writes to parent)
             *   - write end of its control pipe (child only reads approvals)  */
            close(travelers[i].pipeFd[0]);
            close(travelers[i].controlPipe[1]);

            /* Hand over to the child loop; childMain never returns. */
            childMain(filename,
                      i,
                      travelers[i].src,
                      travelers[i].dest,
                      travelers[i].pipeFd[1],      /* child writes here */
                      travelers[i].controlPipe[0]);/* child reads here  */
        }

        /* ---- PARENT branch (fork returned the child's pid) ---- */
        travelers[i].pid = pid;

        /* Parent closes the ends IT does not use (the child's ends). */
        close(travelers[i].pipeFd[1]);
        travelers[i].pipeFd[1] = -1;

        close(travelers[i].controlPipe[0]);
        travelers[i].controlPipe[0] = -1;

        /* Make the parent's read end NON-BLOCKING, so reading an empty pipe
         * returns immediately instead of freezing the 60-FPS render loop. */
        int flags = fcntl(travelers[i].pipeFd[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(travelers[i].pipeFd[0], F_SETFL, flags | O_NONBLOCK);
        }
    }

    return 1;
}

/* Main GUI entry point: load the graph, spawn travelers, run scheduling, and draw everything. */
/* ========================= PARENT / GUI ENTRY POINT =========================
 * The top-level function called from main(). It:
 *   1. loads the graph and reads the traveler list from the file
 *   2. runs Dijkstra for each traveler, then spawnTravelers() forks the children
 *   3. opens the window and enters the 60-FPS loop, where each frame it:
 *        - drains child messages (processTravelerMessages)
 *        - runs the scheduler (dispatchReadyNodes)
 *        - reaps finished children
 *        - handles Play/Pause/Reset clicks (sends SIGUSR1/SIGSTOP/SIGCONT)
 *        - draws the graph, travelers, and side panel
 *   4. on exit, kills/reaps every child and frees memory
 * ========================================================================== */
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
    SetTargetFPS(60);                       /* run the loop ~60 times per second */
    computeLayout(&vg, W, H);               /* place nodes in a circle           */
    Font font = GetFontDefault();

    /* ---- MAIN LOOP: runs once per frame until the window is closed ---- */
    while (!WindowShouldClose()) {
        double now = GetTime();             /* seconds since the window opened   */

        /* 1) read everything the children have told us since last frame */
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
                /* Figure out if travelers are already running (Play vs Pause/Resume). */
                int anyStarted = 0;
                for (int i = 0; i < numTravelers; i++) {
                    if (travelers[i].started && !travelers[i].finished) {
                        anyStarted = 1;
                        break;
                    }
                }

                if (!anyStarted) {
                    /* FIRST PLAY: wake every child out of pause() with SIGUSR1. */
                    for (int i = 0; i < numTravelers; i++) {
                        Traveler *t = &travelers[i];
                        if (!t->started && t->pid > 0) {
                            kill(t->pid, SIGUSR1);   /* the "start" signal */
                            t->started = 1;
                        }
                    }
                    isPaused = 0;
                } else if (!isPaused) {
                    /* PAUSE: freeze each running child with SIGSTOP. */
                    for (int i = 0; i < numTravelers; i++) {
                        Traveler *t = &travelers[i];
                        if (t->started && !t->finished && t->pid > 0) {
                            kill(t->pid, SIGSTOP);   /* freeze the process */
                        }
                        if (t->moving) {
                            /* remember how far along the edge animation was */
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
                    /* RESUME: unfreeze each child with SIGCONT. */
                    for (int i = 0; i < numTravelers; i++) {
                        Traveler *t = &travelers[i];
                        if (t->started && !t->finished && t->pid > 0) {
                            kill(t->pid, SIGCONT);   /* resume the process */
                        }
                        if (t->moving) {
                            /* rebase the animation clock so motion continues smoothly */
                            t->moveStartTime = GetTime() - t->pausedProgress;
                        }
                    }
                    isPaused = 0;
                }
            }

            if (CheckCollisionPointRec(mouse, resetButton)) {
                /* RESET: kill all current children (SIGTERM via cleanup), wipe
                 * their state, then re-fork a fresh batch and clear the queues. */
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

    /* Shutdown: kill+reap every child (SIGTERM via cleanup) and free all memory,
     * so no orphaned child processes or leaks are left behind. */
    for (int i = 0; i < numTravelers; i++) {
        cleanupTravelerProcess(&travelers[i]);
        freeDijkstraResult(&travelers[i].result);
    }

    free(travelers);
    freeGraph(algoGraph);
}
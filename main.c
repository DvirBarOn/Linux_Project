#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Dijkstra.h"

typedef enum {
    SCHED_FCFS,
    SCHED_SJF,
    SCHED_PRIORITY
} SchedulerType;

/* declaration from GraphVisual.c */
void runGraphVisualizer(const char *filename, SchedulerType scheduler);

static void printUsage(const char *progName) {
    fprintf(stderr,
            "Usage:\n"
            "  %s -schd fcfs <path-to-graph-file>\n"
            "  %s -schd sjf <path-to-graph-file>\n"
            "  %s -schd priority <path-to-graph-file>\n",
            progName, progName, progName);
}

int main(int argc, char *argv[]) {
    const char *userArg = NULL;
    SchedulerType scheduler;

    if (argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-schd") != 0) {
        printUsage(argv[0]);
        return 1;
    }

    if (strcmp(argv[2], "fcfs") == 0) {
        scheduler = SCHED_FCFS;
    }
    else if (strcmp(argv[2], "sjf") == 0) {
        scheduler = SCHED_SJF;
    }
    else if (strcmp(argv[2], "priority") == 0) {
        scheduler = SCHED_PRIORITY;
    }
    else {
        fprintf(stderr, "Unknown scheduler: %s\n", argv[2]);
        printUsage(argv[0]);
        return 1;
    }

    userArg = argv[3];

    char *path = resolveGraphPath(userArg);
    if (path == NULL) {
        fprintf(stderr,
                "Could not find graph file.\n"
                "Searched: command-line arg, ./Graph.txt, next to the executable, "
                "and the source directory.\n");
        return 1;
    }

    runGraphVisualizer(path, scheduler);
    free(path);
    return 0;
}
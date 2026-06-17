# Operating Systems Project – Graph Simulation

A directed weighted graph simulator with Dijkstra shortest paths, raylib
visualization, and a multi-traveler process model using fork(), IPC, node
synchronization, and scheduling policies.

## Participants

- Dvir Baron
- Itay Chapnitsky
- Noam Cohen
- Elie Bensoussan

---

## Quick start

```bash
git clone https://github.com/DvirBarOn/Linux_Project.git
cd Linux_Project
cmake -B build
cmake --build build
./build/sim -schd fcfs
```

The first build may take 1–2 minutes because raylib is downloaded and compiled
automatically if it is not already available on the system.

---

## Platform support

| Platform                          | Status           |
| --------------------------------- | ---------------- |
| Linux (Ubuntu, Fedora, Arch, ...) | ✅ supported     |
| macOS (Intel and Apple Silicon)   | ✅ supported     |
| WSL (Linux on Windows)            | ✅ supported     |
| Native Windows                    | ❌ not supported |

Native Windows is not supported because the project uses POSIX process APIs
such as `fork`, `kill`, and `waitpid`, which are not available on Windows.
Windows users should build and run under WSL.

---

## Requirements

- CMake 3.14 or newer
- A C compiler (`gcc` or `clang`)
- Internet access on the first build (to download raylib)

On Linux, if raylib is built from source, the OpenGL/X11 development packages
are also needed. On Ubuntu:

```bash
sudo apt-get install -y build-essential cmake \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev
```

On macOS, Xcode Command Line Tools are enough.

---

## Build targets

CMake produces the following binaries in `build/`:

| Binary     | Description                                               |
| ---------- |-----------------------------------------------------------|
| `dijkstra` | Milestone 1 — terminal Dijkstra                           |
| `sim`      | Milestone 5-7 — GUI simulation with IPC and scheduling |
| `sim4`     | Milestone 4 — multi-traveler with `fork()`                |

> Note: in the current project layout, milestones 2, 3, 5, 6, and 7 run through sim.

---

## Running

Every binary auto-discovers `Graph.txt`. The search order is:

1. The command-line argument, if one is given
2. `./Graph.txt` in the current working directory
3. `Graph.txt` next to the executable
4. `Graph.txt` in the source directory

Examples:

```bash
./build/sim
./build/sim mygraph.txt
cd build && ./sim
```
## Milestone 7 usage

Run the simulator with a scheduling policy:
./sim -schd fcfs Graph.txt
./sim -schd sjf Graph.txt

* FCFS selects the traveler that requested the node first
* SJF selects the traveler whose next edge has the smallest weight
---

## Input file format

```text
# graph definition
N M               # N vertices, M edges
u v w             # M lines: edge from u to v with weight w
...

# travelers (milestones 4, 5, 6 and 7)
T                 # number of travelers
src dest          # T lines: source / destination for each traveler
src dest
...
```

For milestone 1, the file ends with a single `src dest` line instead of the
traveler block.

Lines beginning with `#` are treated as comments and ignored.

---

## Milestone overview

### Milestone 1 — Dijkstra in terminal
- Load a directed weighted graph from file
- Run Dijkstra from source to destination
- Print the shortest path and total distance

### Milestone 4 — Multi-traveler process model
- Create one child process per traveler using `fork()`
- Parent manages the GUI
- Multiple travelers move simultaneously on the graph
- Parent terminates each child when its traveler reaches the destination

### Milestone 5 — IPC-based autonomous travelers
- Each child process computes its own shortest path independently
- Each child reports progress to the parent whenever it reaches a new node
- The parent receives these messages through IPC
- The GUI is updated according to the messages sent by the children
- The terminal log is printed by the parent based on received IPC messages
- Travelers move visually according to edge weights

---



### Milestone 6 — Synchronized node access
- Added synchronization so that only **one traveler can stay inside a node at any given time**
- Each node is treated as a shared resource protected by a **POSIX semaphore**
- If a traveler reaches an occupied node, it waits **outside the node** until the node becomes available
- After entering a node, the traveler stays inside it for **1 full second**
- Once the traveler leaves the node, it releases the semaphore so another traveler may enter
- The GUI now distinguishes between travelers that are **waiting outside a node**, **inside a node**, **moving on an edge**, and **finished**

---

### Milestone 7 — Node scheduling policies

* Added node-level scheduling policies for travelers waiting to enter a node
* The scheduler is selected at runtime using a command-line flag
* Supported schedulers:
  * FCFS — First Come First Served
  * SJF — Shortest Job First
* In this project, the SJF job length is defined as the weight of the next edge
* When multiple travelers wait for the same node, the parent process chooses who enters next according to the selected scheduler
* A short collection window is used so the parent can gather competing requests before making the scheduling decision
* The active scheduler is displayed in the GUI

---

## Synchronization choice

For milestone 6 we chose **POSIX semaphores**, with **one semaphore per node**.

### Why semaphores?
- Each node behaves like a shared resource with capacity 1
- A semaphore models this naturally: a traveler must acquire the node before entering it
- If the node is already occupied, the traveler blocks and waits outside
- This mechanism works well with a multi-process design based on `fork()`

In the implementation:
- `sem_wait` / `sem_trywait` are used before entering a node
- The traveler remains inside the node for one second
- `sem_post` is called when leaving the node
- This guarantees that no two travelers are inside the same node at the same time

---
## Process model (milestones 5–7)

The simulator uses a parent-child architecture.

Parent process

The parent:

1. Loads the graph and traveler list
2. Creates one child process per traveler
3. Creates IPC channels between parent and children
4. Runs the raylib GUI loop
5. Receives traveler updates from children
6. Updates the visualization and terminal output
7. Handles Play / Pause / Reset interactions
8. Applies synchronization and scheduling logic when travelers compete for nodes

Child processes

Each child:

1. Waits for the start signal from the parent
2. Loads the graph independently
3. Runs Dijkstra independently for its own source and destination
4. Sends status messages to the parent
5. Waits for parent approval before entering a node when required
6. Sends a final message when it finishes

---

## IPC choice

For milestone 5 we chose **pipes** as the IPC mechanism.

### Why pipes?
- They are simple and fit naturally into a parent-child design created by `fork()`
- Each child only needs to send short status updates to the parent
- The communication is one-directional, which matches the project logic well:
  child → parent
- Pipes are lightweight and easy to integrate with process-based execution

Each child writes structured progress messages into its pipe, and the parent
reads these messages in non-blocking mode and updates the GUI accordingly.

---

## Synchronization and scheduling

Synchronization

Milestone 6 introduced synchronized access to nodes so that only one traveler
can stay inside a node at a time.

Scheduling

Milestone 7 extends this behavior by deciding which waiting traveler enters next
when several travelers compete for the same node.

Two scheduling policies are supported:

* FCFS — selects the traveler that requested the node first
* SJF — selects the traveler whose next edge has the smallest weight

To reduce timing bias caused by process creation and message arrival order, the
parent uses a short collection window before dispatching the next traveler
into a free node.

---

## GUI controls (milestones 5–7)

The GUI includes:

* Play — starts the travelers and later acts as Pause / Resume
* Reset — resets the simulation, recreates the child processes, and returns all travelers to their starting nodes

Traveler movement speed depends on the weight of the edge currently being crossed.

The GUI also displays the active scheduling policy in milestone 7.

---

## Example milestone 7 terminal output

```text
[PID=9001] request node 3 | next node: 4 | edge weight: 9
[PID=9002] request node 3 | next node: 5 | edge weight: 2
[PID=9002] entered node 3 | next node: 5
[PID=9002] leaving node 3 | moving to node 5
[PID=9001] entered node 3 | next node: 4
```

---

## Building without CMake

A fallback `Makefile` is also included. It assumes raylib is already installed
on the system:

```bash
# macOS:   brew install raylib
# Ubuntu:  sudo apt-get install libraylib-dev   (or build raylib from source)

make milestone1   # builds ./dijkstra
make milestone2   # builds ./sim
make milestone3   # builds ./sim
make milestone4   # builds ./sim4
make milestone5   # builds ./sim
make milestone6   # builds ./sim
make milestone7   # builds ./sim
```

The CMake build is the recommended option because it handles raylib
automatically.

---

## Repository layout

```text
Linux_Project/
├── CMakeLists.txt    # build config (handles raylib auto-fetch)
├── Makefile          # alternative build path
├── Dijkstra.h        # shared types and function declarations
├── Dijkstra.c        # graph, Dijkstra, loaders, helpers
├── Dijkstra_main.c   # milestone 1 entry point
├── GraphVisual.c     # GUI + processes + IPC + scheduling
├── main.c            # simulator entry point
├── Graph.txt         # example input
└── README.md
```

---

## Notes

* Milestone 5 extends milestone 4 by moving shortest-path responsibility into the child processes themselves
* Milestone 6 extends milestone 5 by adding synchronized access to nodes
* Milestone 7 extends milestone 6 by adding FCFS and SJF scheduling for travelers waiting to enter a node
* The parent reacts to IPC messages sent by the children and updates the GUI accordingly
* The simulation is designed for POSIX-compatible systems only

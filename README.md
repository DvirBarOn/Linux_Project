# Operating Systems Project – Graph Simulation

A directed weighted graph simulator with Dijkstra shortest paths, raylib
visualization, and a multi-traveler process model using `fork()` and IPC.

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
./build/sim
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

| Binary     | Description                                          |
| ---------- | ---------------------------------------------------- |
| `dijkstra` | Milestone 1 — terminal Dijkstra                      |
| `sim`      | Milestone 5 — IPC-based multi-traveler visualization |
| `sim4`     | Milestone 4 — multi-traveler with `fork()`           |

> Note: in the current project layout, milestone 5 runs through `sim`.

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

---

## Input file format

```text
# graph definition
N M               # N vertices, M edges
u v w             # M lines: edge from u to v with weight w
...

# travelers (milestones 4 and 5)
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

## Process model (milestone 5)

Milestone 5 uses a parent-child architecture.

### Parent process
The parent:
1. Loads the graph and traveler list
2. Creates one child process per traveler
3. Creates IPC channels between parent and children
4. Runs the raylib GUI loop
5. Receives traveler updates from children
6. Updates the visualization and terminal output
7. Handles Play / Pause / Reset interactions

### Child processes
Each child:
1. Waits for the start signal from the parent
2. Loads the graph independently
3. Runs Dijkstra independently for its own source and destination
4. Sends a message to the parent whenever it reaches a node
5. Sends a final message when it finishes

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

## GUI controls (milestone 5)

The GUI includes:

- **Play** — starts the travelers and later acts as Pause / Resume
- **Reset** — resets the simulation, recreates the child processes, and returns all travelers to their starting nodes

Traveler movement speed depends on the **weight of the edge** currently being crossed.

---

## Example milestone 5 terminal output

```text
[PID=2314] arrived at node 0 | next node: 2
[PID=2315] arrived at node 1 | next node: 4
[PID=2314] arrived at node 2 | next node: 5
[PID=2315] arrived at node 4 | DESTINATION
[PID=2315] finished
[PID=2314] arrived at node 5 | DESTINATION
[PID=2314] finished
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
├── GraphVisual.c     # GUI + processes + IPC + animation
├── main.c            # simulator entry point
├── Graph.txt         # example input
└── README.md
```

---

## Notes

- Milestone 5 extends milestone 4 by moving shortest-path responsibility into the child processes themselves
- The parent reacts to IPC messages sent by the children and updates the GUI accordingly
- The simulation is designed for POSIX-compatible systems only

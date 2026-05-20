# Operating Systems Project – Graph Simulation

A directed weighted graph simulator with Dijkstra shortest paths, raylib
visualization, and a multi-traveler process model using `fork()`.

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
./build/sim4
```

That's it. The first build will take ~1–2 minutes because raylib is downloaded
and compiled automatically if it isn't already on the system.

## Platform support

| Platform                          | Status                |
| --------------------------------- | --------------------- |
| Linux (Ubuntu, Fedora, Arch, ...) | ✅ supported          |
| macOS (Intel and Apple Silicon)   | ✅ supported          |
| WSL (Linux on Windows)            | ✅ supported          |
| Native Windows                    | ❌ not supported      |

Native Windows is not supported because the project uses POSIX process APIs
(`fork`, `kill`, `waitpid`) which do not exist on Windows. Windows users
should build under WSL.

## Requirements

- CMake 3.14 or newer
- A C compiler (gcc or clang)
- Internet access on the first build (to download raylib)

On Linux you also need raylib's X11/OpenGL dev headers if you want to build
raylib from source. On a fresh Ubuntu:

```bash
sudo apt-get install -y build-essential cmake \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev
```

macOS needs nothing extra — Xcode Command Line Tools are enough.

## Build targets

CMake produces three binaries in `build/`:

| Binary          | Description                                  |
| --------------- | -------------------------------------------- |
| `dijkstra`      | Milestone 1 — terminal Dijkstra              |
| `sim`           | Milestone 3 — single-traveler GUI            |
| `sim4`          | Milestone 4 — multi-traveler with `fork()`   |

`sim` and `sim4` build from the same source in this layout; both run the
multi-traveler visualizer. Use `sim4` if you want the milestone-4-specific
name.

## Running

Every binary auto-discovers `Graph.txt`. The search order is:

1. The command-line argument, if you pass one
2. `./Graph.txt` in the current directory
3. `Graph.txt` next to the executable
4. `Graph.txt` in the source directory (baked in at build time)

So all of these work after building:

```bash
./build/sim4                          # uses Graph.txt from source dir
./build/sim4 mygraph.txt              # use a different file
cd build && ./sim4                    # finds Graph.txt next to itself
```

## Input file format

```
# graph definition
N M               # N vertices, M edges
u v w             # M lines: edge from u to v with weight w
...
# travelers (milestone 4 only)
T                 # number of travelers
src dest          # T lines: source / destination for each traveler
src dest
...
```

For milestone 1 the file ends with a single `src dest` line instead of the
traveler block.

Lines beginning with `#` are comments and are ignored everywhere in the file.

## Process model (milestone 4)

The parent process:

1. Reads the graph and traveler list
2. Computes a Dijkstra path for each traveler
3. `fork()`s one child per traveler
4. Runs the raylib loop and draws all travelers moving in parallel, each in
   a unique color
5. When a traveler arrives, sends `SIGTERM` to its child and reaps it with
   `waitpid()`

Each child:

1. Prints `[PID] started`
2. Sleeps on `pause()` until signaled by the parent
3. Exits cleanly

## Building without CMake

If you prefer plain make, a Makefile is included as a fallback. It assumes
raylib is already installed system-wide:

```bash
# macOS:   brew install raylib
# Ubuntu:  sudo apt-get install libraylib-dev   (or build raylib from source)

make milestone1     # builds ./dijkstra
make milestone3     # builds ./sim
make milestone4     # builds ./sim4
```

The CMake build is the recommended path because it handles raylib automatically.

## Repository layout

```
Linux_Project/
├── CMakeLists.txt    # build config (handles raylib auto-fetch)
├── Makefile          # alternative build, requires system raylib
├── Dijkstra.h        # shared types and prototypes
├── Dijkstra.c        # graph, algorithm, file loaders, path resolution
├── Dijkstra_main.c   # milestone 1 entry point
├── GraphVisual.c     # raylib visualization + fork/signal logic
├── main.c            # milestone 3/4 entry point
├── Graph.txt         # example input
└── README.md
```

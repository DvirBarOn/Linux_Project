# =============================================================================
#  Linux_Project — Makefile with automatic raylib provisioning
#
#  Targets you care about:
#    make milestone1     -> ./dijkstra      (terminal Dijkstra, no raylib)
#    make milestone4     -> ./sim4          (multi-traveler GUI)
#    make milestone7     -> ./sim           (full GUI + scheduling)
#    make                -> builds everything (alias for `make all`)
#    make setup          -> install system deps + prebuild raylib (run once on a
#                           fresh machine; uses sudo on Linux)
#    make system-deps    -> install the X11/OpenGL dev libraries only
#    make clean          -> remove our binaries
#    make distclean      -> also remove the downloaded/built raylib
#
#  raylib is handled automatically: if it is not already installed on the
#  system, it is downloaded from GitHub and built locally into external/raylib.
#  Works on Linux and macOS. Native Windows is not supported (use WSL).
# =============================================================================

# ---------- toolchain ----------
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11

UNAME_S := $(shell uname -s)

# ---------- raylib auto-provisioning ----------
RAYLIB_VERSION := 5.0
RAYLIB_DIR     := external/raylib
RAYLIB_SRC     := $(RAYLIB_DIR)/src
RAYLIB_LIB     := $(RAYLIB_SRC)/libraylib.a

# Prefer a raylib already installed on the system (via pkg-config).
HAVE_SYS_RAYLIB := $(shell pkg-config --exists raylib 2>/dev/null && echo yes)

ifeq ($(HAVE_SYS_RAYLIB),yes)
    # ----- use system raylib -----
    RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
    RAYLIB_LIBS   := $(shell pkg-config --libs raylib)
    RAYLIB_DEP    :=
else
    # ----- use locally-built static raylib -----
    RAYLIB_CFLAGS := -I$(RAYLIB_SRC)
    RAYLIB_DEP    := $(RAYLIB_LIB)
    ifeq ($(UNAME_S),Darwin)
        RAYLIB_LIBS := $(RAYLIB_LIB) -lm -lpthread \
                       -framework CoreVideo -framework IOKit -framework Cocoa \
                       -framework GLUT -framework OpenGL
    else
        RAYLIB_LIBS := $(RAYLIB_LIB) -lm -lpthread -ldl -lrt -lX11 -lGL
    endif
endif

# ---------- top-level ----------
all: milestone1 milestone7
	@echo ">> Built dijkstra, sim and sim4."

# ---------- milestone 1: terminal Dijkstra (no raylib) ----------
milestone1: dijkstra

dijkstra: Dijkstra.c Dijkstra_main.c Dijkstra.h
	$(CC) $(CFLAGS) Dijkstra.c Dijkstra_main.c -o dijkstra

# ---------- milestones 2,3,5,6,7: GUI simulator ----------
milestone2: sim
milestone3: sim
milestone5: sim
milestone6: sim
milestone7: sim

sim: Dijkstra.c GraphVisual.c main.c Dijkstra.h $(RAYLIB_DEP)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim $(RAYLIB_LIBS)

# ---------- milestone 4: multi-traveler + fork ----------
milestone4: sim4

sim4: Dijkstra.c GraphVisual.c main.c Dijkstra.h $(RAYLIB_DEP)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim4 $(RAYLIB_LIBS)

# ---------- raylib: fetch + build locally if no system copy ----------
$(RAYLIB_LIB):
	@echo ">> System raylib not found. Provisioning raylib $(RAYLIB_VERSION) locally..."
	@$(MAKE) --no-print-directory ensure-system-deps
	@if [ ! -f "$(RAYLIB_SRC)/Makefile" ]; then \
		echo ">> Cloning raylib $(RAYLIB_VERSION) from GitHub..."; \
		git clone --depth 1 --branch $(RAYLIB_VERSION) \
			https://github.com/raysan5/raylib.git $(RAYLIB_DIR); \
	fi
	@echo ">> Building raylib static library..."
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@echo ">> raylib ready at $(RAYLIB_LIB)"

# ---------- system dependency helpers ----------
# Only installs when the headers are actually missing, and only on Linux.
ensure-system-deps:
ifeq ($(UNAME_S),Linux)
	@if [ ! -f /usr/include/GL/gl.h ] || [ ! -f /usr/include/X11/Xlib.h ]; then \
		echo ">> X11/OpenGL dev headers missing — installing them (needs sudo)..."; \
		$(MAKE) --no-print-directory system-deps; \
	else \
		echo ">> System dev headers present."; \
	fi
else
	@true
endif

# Detects the platform's package manager and installs raylib's build deps.
system-deps:
	@case "$(UNAME_S)" in \
	  Linux) \
	    if command -v apt-get >/dev/null 2>&1; then \
	      sudo apt-get update && sudo apt-get install -y build-essential git \
	        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
	        libgl1-mesa-dev libglu1-mesa-dev ; \
	    elif command -v dnf >/dev/null 2>&1; then \
	      sudo dnf install -y gcc make git libX11-devel libXrandr-devel \
	        libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel ; \
	    elif command -v pacman >/dev/null 2>&1; then \
	      sudo pacman -S --needed --noconfirm base-devel git libx11 libxrandr \
	        libxinerama libxcursor libxi mesa ; \
	    else \
	      echo "!! Could not detect apt/dnf/pacman. Install X11 + OpenGL dev libs manually."; \
	      exit 1; \
	    fi ;; \
	  Darwin) \
	    echo ">> macOS: Xcode Command Line Tools supply the needed frameworks."; \
	    xcode-select -p >/dev/null 2>&1 || xcode-select --install ;; \
	  *) \
	    echo "!! Unsupported OS '$(UNAME_S)'. On Windows, build under WSL."; \
	    exit 1 ;; \
	esac

# One-shot setup for a fresh machine: deps + prebuilt raylib.
setup:
	@$(MAKE) --no-print-directory ensure-system-deps
	@$(MAKE) --no-print-directory $(RAYLIB_LIB) || true
	@echo ">> Setup complete. Now run: make milestone7 && ./sim -schd fcfs Graph.txt"

# ---------- housekeeping ----------
clean:
	rm -f dijkstra sim sim4 *.o

distclean: clean
	rm -rf external

.PHONY: all milestone1 milestone2 milestone3 milestone4 milestone5 milestone6 \
        milestone7 clean distclean setup system-deps ensure-system-deps
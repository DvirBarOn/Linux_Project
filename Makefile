# =============================================================================
#  Linux_Project — Makefile with automatic raylib provisioning
#
#  Common targets:
#    make milestone1     -> ./dijkstra   (terminal Dijkstra, no raylib)
#    make milestone4     -> ./sim4       (multi-traveler GUI)
#    make milestone7     -> ./sim        (full GUI + scheduling)
#    make                -> builds dijkstra + sim
#    make system-deps    -> install the X11/OpenGL dev libraries (needs root;
#                           run this ONCE on a fresh machine)
#    make setup          -> system-deps + prebuild raylib
#    make clean          -> remove our binaries
#    make distclean      -> also remove the downloaded/built raylib
#
#  raylib is handled automatically: if it is not already installed on the
#  system, it is downloaded from GitHub and built locally into external/raylib.
#  Works on Linux and macOS. Native Windows is not supported (use WSL).
#
#  NOTE: installing the X11/OpenGL *system* libraries needs root, which a build
#  launched from an IDE cannot do. The build never calls sudo itself: if those
#  headers are missing it tells you to run `make system-deps` once. That target
#  uses sudo in a terminal, or a graphical password prompt (pkexec) from an IDE.
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
	@echo ">> Built dijkstra and sim."

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
	@$(MAKE) --no-print-directory check-headers
	@if [ ! -f "$(RAYLIB_SRC)/Makefile" ]; then \
		echo ">> Cloning raylib $(RAYLIB_VERSION) from GitHub..."; \
		git clone --depth 1 --branch $(RAYLIB_VERSION) \
			https://github.com/raysan5/raylib.git $(RAYLIB_DIR); \
	fi
	@echo ">> Building raylib static library..."
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@echo ">> raylib ready at $(RAYLIB_LIB)"

# ---------- header check (called by the build; NEVER runs sudo) ----------
# If the X11/OpenGL dev headers are missing, stop with a clear instruction
# instead of trying to install them from inside the build.
check-headers:
ifeq ($(UNAME_S),Linux)
	@if [ -f /usr/include/GL/gl.h ] && [ -f /usr/include/X11/Xlib.h ]; then \
		echo ">> System dev headers present."; \
	else \
		printf '%s\n' \
		  "" \
		  "!! ----------------------------------------------------------------" \
		  "!! X11 / OpenGL development headers are not installed yet." \
		  "!! Installing them needs root, which a build cannot do on its own." \
		  "!!" \
		  "!! Run this ONCE, then build again:" \
		  "!!" \
		  "!!     make system-deps" \
		  "!!" \
		  "!! From a terminal it asks for your password; from an IDE it opens" \
		  "!! a graphical password dialog. Either way, it is a one-time step." \
		  "!! ----------------------------------------------------------------" \
		  "" ; \
		exit 1; \
	fi
else
	@true
endif

# ---------- system dependencies (run by YOU, may use sudo or a GUI prompt) ----------
# Detects the package manager, then picks the right way to get root:
#   running as root  -> no escalation
#   real terminal    -> sudo (asks for password in the terminal)
#   IDE with display -> pkexec (graphical password dialog)
#   otherwise        -> sudo, or a printed manual command
system-deps:
	@case "$(UNAME_S)" in \
	  Linux) \
	    if command -v apt-get >/dev/null 2>&1; then \
	      PKG='apt-get update || true; apt-get install -y build-essential git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev'; \
	    elif command -v dnf >/dev/null 2>&1; then \
	      PKG='dnf install -y gcc make git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel'; \
	    elif command -v pacman >/dev/null 2>&1; then \
	      PKG='pacman -S --needed --noconfirm base-devel git libx11 libxrandr libxinerama libxcursor libxi mesa'; \
	    else \
	      echo "!! No apt/dnf/pacman found. Install X11 + OpenGL dev libraries manually."; exit 1; \
	    fi; \
	    if [ "$$(id -u)" = "0" ]; then \
	      sh -c "$$PKG"; \
	    elif [ -t 0 ] && command -v sudo >/dev/null 2>&1; then \
	      echo ">> Installing dev headers (sudo)..."; sudo sh -c "$$PKG"; \
	    elif command -v pkexec >/dev/null 2>&1 && [ -n "$$DISPLAY" ]; then \
	      echo ">> Installing dev headers via graphical password prompt (pkexec)..."; pkexec sh -c "$$PKG"; \
	    elif command -v sudo >/dev/null 2>&1; then \
	      echo ">> Installing dev headers (sudo)..."; sudo sh -c "$$PKG"; \
	    else \
	      echo "!! Need root. In a terminal run:  sudo sh -c \"$$PKG\""; exit 1; \
	    fi; \
	    echo ">> System dependencies installed." ;; \
	  Darwin) \
	    echo ">> macOS: Xcode Command Line Tools supply the needed frameworks."; \
	    xcode-select -p >/dev/null 2>&1 || xcode-select --install ;; \
	  *) \
	    echo "!! Unsupported OS '$(UNAME_S)'. On Windows, build under WSL."; \
	    exit 1 ;; \
	esac

# One-shot setup for a fresh machine: deps + prebuilt raylib.
setup: system-deps
	@$(MAKE) --no-print-directory $(RAYLIB_LIB)
	@echo ">> Setup complete. Now run: make milestone7 && ./sim -schd fcfs Graph.txt"

# ---------- housekeeping ----------
clean:
	rm -f dijkstra sim sim4 *.o

distclean: clean
	rm -rf external

.PHONY: all milestone1 milestone2 milestone3 milestone4 milestone5 milestone6 \
        milestone7 clean distclean setup system-deps check-headers
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11
UNAME_S := $(shell uname -s)

# raylib's own dependencies — needed when linking the STATIC libraylib, and
# harmless when linking the shared one. Always appended after -lraylib.
ifeq ($(UNAME_S),Darwin)
    PLATFORM_LIBS := -lm -lpthread -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
else
    PLATFORM_LIBS := -lm -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXcursor -lXinerama -lGL
endif

RAYLIB_VERSION := 5.0
RAYLIB_DIR     := external/raylib
RAYLIB_SRC     := $(RAYLIB_DIR)/src
RAYLIB_LIB     := $(RAYLIB_SRC)/libraylib.a

# --- raylib detection, in priority order ---
# 1) pkg-config (a proper system install)
HAVE_PC := $(shell pkg-config --exists raylib 2>/dev/null && echo yes)
# 2) a from-source install (header + lib) under a common prefix, e.g. /usr/local
RAYLIB_PREFIX := $(shell for p in /usr/local /usr /opt/homebrew $(HOME)/.local; do \
    if [ -f "$$p/include/raylib.h" ] && ls "$$p"/lib/libraylib.* >/dev/null 2>&1; then echo "$$p"; break; fi; \
  done)

ifeq ($(HAVE_PC),yes)
    # ----- system raylib via pkg-config -----
    RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
    RAYLIB_LIBS   := $(shell pkg-config --libs raylib) $(PLATFORM_LIBS)
    RAYLIB_DEP    :=
    RAYLIB_VIA    := pkg-config
else ifneq ($(RAYLIB_PREFIX),)
    # ----- system raylib found by path (no pkg-config, e.g. /usr/local) -----
    RAYLIB_CFLAGS := -I$(RAYLIB_PREFIX)/include
    RAYLIB_DEP    :=
    RAYLIB_VIA    := $(RAYLIB_PREFIX)
    ifeq ($(UNAME_S),Darwin)
        RAYLIB_LIBS := -L$(RAYLIB_PREFIX)/lib -Wl,-rpath,$(RAYLIB_PREFIX)/lib -lraylib $(PLATFORM_LIBS)
    else
        RAYLIB_LIBS := -L$(RAYLIB_PREFIX)/lib -Wl,-rpath,$(RAYLIB_PREFIX)/lib -lraylib $(PLATFORM_LIBS)
    endif
else
    # ----- no system raylib: build it locally into external/ -----
    RAYLIB_CFLAGS := -I$(RAYLIB_SRC)
    RAYLIB_DEP    := $(RAYLIB_LIB)
    RAYLIB_VIA    := local-build
    ifeq ($(UNAME_S),Darwin)
        RAYLIB_LIBS := $(RAYLIB_LIB) $(PLATFORM_LIBS)
    else
        RAYLIB_LIBS := $(RAYLIB_LIB) $(PLATFORM_LIBS)
    endif
endif

raylib-info:
	@echo "raylib source : $(RAYLIB_VIA)"
	@echo "CFLAGS        : $(RAYLIB_CFLAGS)"
	@echo "LIBS          : $(RAYLIB_LIBS)"

all: milestone1 milestone7
milestone1: dijkstra
dijkstra: Dijkstra.c Dijkstra_main.c Dijkstra.h
	$(CC) $(CFLAGS) Dijkstra.c Dijkstra_main.c -o dijkstra
milestone2: sim
milestone3: sim
milestone5: sim
milestone6: sim
milestone7: sim
sim: Dijkstra.c GraphVisual.c main.c Dijkstra.h $(RAYLIB_DEP)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim $(RAYLIB_LIBS)
milestone4: sim4
sim4: Dijkstra.c GraphVisual.c main.c Dijkstra.h $(RAYLIB_DEP)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) Dijkstra.c GraphVisual.c main.c -o sim4 $(RAYLIB_LIBS)

$(RAYLIB_LIB):
	@echo ">> Provisioning raylib $(RAYLIB_VERSION) locally..."
	@$(MAKE) --no-print-directory check-headers
	@if [ ! -f "$(RAYLIB_SRC)/Makefile" ]; then \
		git clone --depth 1 --branch $(RAYLIB_VERSION) https://github.com/raysan5/raylib.git $(RAYLIB_DIR); \
	fi
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@echo ">> raylib ready."

check-headers:
ifeq ($(UNAME_S),Linux)
	@if [ -f /usr/include/GL/gl.h ] && [ -f /usr/include/X11/Xlib.h ]; then \
		echo ">> System dev headers present."; \
	else echo "!! X11/OpenGL headers missing. Run: make system-deps"; exit 1; fi
else
	@true
endif

system-deps:
	@if command -v apt-get >/dev/null 2>&1; then \
	  PKG='apt-get update || true; apt-get install -y build-essential git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev'; \
	else echo "Install X11+GL dev libs manually."; exit 1; fi; \
	if [ "$$(id -u)" = "0" ]; then sh -c "$$PKG"; \
	elif [ -t 0 ] && command -v sudo >/dev/null 2>&1; then sudo sh -c "$$PKG"; \
	elif command -v pkexec >/dev/null 2>&1 && [ -n "$$DISPLAY" ]; then pkexec sh -c "$$PKG"; \
	elif command -v sudo >/dev/null 2>&1; then sudo sh -c "$$PKG"; \
	else echo "Need root. Run: sudo sh -c \"$$PKG\""; exit 1; fi

setup: system-deps
	@$(MAKE) --no-print-directory $(RAYLIB_LIB)
clean:
	rm -f dijkstra sim sim4 *.o
distclean: clean
	rm -rf external
.PHONY: all milestone1 milestone2 milestone3 milestone4 milestone5 milestone6 milestone7 clean distclean setup system-deps check-headers raylib-info
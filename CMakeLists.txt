cmake_minimum_required(VERSION 3.14)
project(Linux_Project C)

# Hard-fail on native Windows. The milestone uses POSIX fork()/kill()/waitpid(),
# which Windows does not provide. Windows users should use WSL.
if(WIN32 AND NOT CYGWIN)
    message(FATAL_ERROR
            "This project uses POSIX process APIs (fork, kill, waitpid) which are "
            "not available on native Windows. Please build under WSL (Windows "
            "Subsystem for Linux), macOS, or Linux instead.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Where Graph.txt lives in the source tree — passed to the program as a
# compile-time fallback path so it can find the file even when run from
# arbitrary working directories.
add_compile_definitions(SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# ----------------------------------------------------------------------------
# raylib: try the system first, then fall back to auto-downloading + building.
# This means: clone the repo and run cmake — no "brew install raylib" required.
# ----------------------------------------------------------------------------
find_package(raylib 4.0 QUIET)

if(NOT raylib_FOUND)
    message(STATUS "raylib not found on system, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG        5.0
            GIT_SHALLOW    TRUE
    )
    # Don't build raylib's examples/games — just the library.
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

# ----------------------------------------------------------------------------
# Targets
# ----------------------------------------------------------------------------

# Milestone 1: terminal Dijkstra
add_executable(dijkstra Dijkstra.c Dijkstra_main.c)

# Milestone 3 / 4: GUI simulator (multi-traveler in our current implementation)
add_executable(sim  Dijkstra.c GraphVisual.c main.c)
add_executable(sim4 Dijkstra.c GraphVisual.c main.c)

target_link_libraries(sim  PRIVATE raylib)
target_link_libraries(sim4 PRIVATE raylib)

# raylib on macOS needs these frameworks. Linux pulls in X11 / GL automatically
# via raylib's own CMake target when built from source.
if(APPLE)
    target_link_libraries(sim  PRIVATE "-framework CoreVideo" "-framework IOKit"
            "-framework Cocoa"     "-framework GLUT"
            "-framework OpenGL")
    target_link_libraries(sim4 PRIVATE "-framework CoreVideo" "-framework IOKit"
            "-framework Cocoa"     "-framework GLUT"
            "-framework OpenGL")
endif()

# Helpful warnings
foreach(t dijkstra sim sim4)
    target_compile_options(${t} PRIVATE -Wall -Wextra -O2)
endforeach()

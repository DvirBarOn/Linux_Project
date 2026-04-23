cmake_minimum_required(VERSION 3.29)
project(Linux_Project C)

set(CMAKE_C_STANDARD 99)

# === raylib paths ===
set(RAYLIB_ROOT "C:/libs/raylib")
set(RAYLIB_INCLUDE_DIR "${RAYLIB_ROOT}/include")
set(RAYLIB_LIB_DIR "${RAYLIB_ROOT}/lib")

# === sources ===
add_executable(Linux_Project
        main.c
        Dijkstra.c
        GraphVisual.c
)

# === include + link directories ===
target_include_directories(Linux_Project PRIVATE "${RAYLIB_INCLUDE_DIR}")
target_link_directories(Linux_Project PRIVATE "${RAYLIB_LIB_DIR}")

# === link libraries ===
target_link_libraries(Linux_Project
        raylib
        opengl32
        gdi32
        winmm
)

# === optional: working directory ===
set_target_properties(Linux_Project PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
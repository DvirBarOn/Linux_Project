cmake_minimum_required(VERSION 3.29)
project(Linux_Project C)

set(CMAKE_C_STANDARD 99)

add_executable(Linux_Project
        main.c
        Dijkstra.c
        GraphVisual.c
)

target_include_directories(Linux_Project PRIVATE /opt/homebrew/include)
target_link_directories(Linux_Project PRIVATE /opt/homebrew/lib)

target_link_libraries(Linux_Project PRIVATE raylib)
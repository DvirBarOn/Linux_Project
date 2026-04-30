cmake_minimum_required(VERSION 3.29)
project(Linux_Project C)

set(CMAKE_C_STANDARD 99)

add_executable(dijkstra
        Dijkstra_main.c
        Dijkstra.c
)

add_executable(sim
        main.c
        Dijkstra.c
        GraphVisual.c
)

target_include_directories(sim PRIVATE /opt/homebrew/include)
target_link_directories(sim PRIVATE /opt/homebrew/lib)
target_link_libraries(sim PRIVATE raylib)
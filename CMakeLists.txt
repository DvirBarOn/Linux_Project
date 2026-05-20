cmake_minimum_required(VERSION 3.29)
project(Linux_Project C)

set(CMAKE_C_STANDARD 99)

add_executable(dijkstra
        Dijkstra_main.c
        Dijkstra.c
        Dijkstra.h
)

add_executable(sim
        main.c
        Dijkstra.c
        GraphVisual.c
)

# Auto-copy Graph.txt to build folder after build
add_custom_command(TARGET dijkstra POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/Graph.txt
        $<TARGET_FILE_DIR:dijkstra>/Graph.txt
)

# Raylib - OS aware
if(APPLE)
    if(EXISTS /opt/homebrew)
        target_include_directories(sim PRIVATE /opt/homebrew/include)
        target_link_directories(sim PRIVATE /opt/homebrew/lib)
    else()
        target_include_directories(sim PRIVATE /usr/local/include)
        target_link_directories(sim PRIVATE /usr/local/lib)
    endif()
endif()

target_link_libraries(sim PRIVATE raylib)
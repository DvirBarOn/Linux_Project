cmake_minimum_required(VERSION 4.0)
project(Project C)

set(CMAKE_C_STANDARD 99)

add_executable(Project main.c)

set_target_properties(Git_hub_C PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Linux_Project/Linux_Project"
)
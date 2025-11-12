cmake_minimum_required(VERSION 3.16)
project(simulated_annealing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

add_library(sa_core STATIC
    src/sa_classes.cpp
)
target_include_directories(sa_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(annealing_solver
    src/annealing_main.cpp
)
target_link_libraries(annealing_solver PRIVATE sa_core)

add_executable(task_generator
    src/generator_main.cpp
)
target_link_libraries(task_generator PRIVATE sa_core)

cmake_minimum_required(VERSION 3.20)
project(mafia LANGUAGES CXX)

if (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/O2 /W4 /permissive-)
else ()
    add_compile_options(-O2 -Wall -Wextra -Wpedantic)
endif ()

set(SRC
        src/logger.cpp
        src/roles.cpp
        src/human.cpp
        src/host.cpp
        src/main.cpp
)

add_executable(mafia ${SRC})
target_include_directories(mafia PRIVATE ${CMAKE_SOURCE_DIR}/include)

find_package(yaml-cpp QUIET)
if (NOT yaml-cpp_FOUND)
    include(FetchContent)
    FetchContent_Declare(
            yaml-cpp
            GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
            GIT_TAG 0.8.0
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif ()

target_link_libraries(mafia PRIVATE yaml-cpp)

find_package(Threads REQUIRED)
target_link_libraries(mafia PRIVATE Threads::Threads)

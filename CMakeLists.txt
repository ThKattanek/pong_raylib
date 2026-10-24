cmake_minimum_required(VERSION 3.14)
project(pong CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib 5.0 QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(pong main.cpp)
target_link_libraries(pong raylib)

if (WIN32)
    # Eigenständige EXE ohne MinGW-DLLs und ohne Konsolenfenster
    target_link_options(pong PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
endif()

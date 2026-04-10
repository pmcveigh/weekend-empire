cmake_minimum_required(VERSION 3.16)
project(weekend_empire LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(weekend_empire
    src/main.cpp
)

target_link_libraries(weekend_empire PRIVATE
    SDL2::SDL2
    OpenGL::GL
)

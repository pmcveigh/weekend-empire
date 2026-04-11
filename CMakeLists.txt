cmake_minimum_required(VERSION 3.16)
project(weekend_empire LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.8-docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/gl3w/GL/gl3w.c
)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/gl3w
)

target_compile_definitions(imgui_lib PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM GL_GLEXT_PROTOTYPES)
target_compile_options(imgui_lib PRIVATE -include SDL2/SDL_opengl.h)

target_link_libraries(imgui_lib PUBLIC SDL2::SDL2 OpenGL::GL)

add_executable(weekend_empire
    src/main.cpp
)

target_link_libraries(weekend_empire PRIVATE
    SDL2::SDL2
    OpenGL::GL
    SQLite::SQLite3
    imgui_lib
)

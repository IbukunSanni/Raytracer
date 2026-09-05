cmake_minimum_required(VERSION 3.16)
project(raytracer CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------
# Vendored dependencies.
#
# This is a CPU renderer: no OpenGL, no windowing, no GUI. It needs
# exactly three third-party pieces, all vendored under third_party/ so
# the repo builds with nothing installed but a compiler and CMake.
#
#   glm     header-only vector/matrix math
#   lodepng PNG encode/decode (2 files)
#   lua     scene description language
# ---------------------------------------------------------------------

# lodepng
add_library(lodepng STATIC third_party/lodepng/lodepng.cpp)
target_include_directories(lodepng PUBLIC third_party)

# lua -- everything in src/ except the two driver programs
file(GLOB LUA_SOURCES third_party/lua-5.3.1/src/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.3.1/src/lua.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.3.1/src/luac.c)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC third_party)

# ---------------------------------------------------------------------
# The renderer
# ---------------------------------------------------------------------

file(GLOB RAYTRACER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(raytracer ${RAYTRACER_SOURCES})

target_include_directories(raytracer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    third_party)

target_link_libraries(raytracer PRIVATE
    lodepng
    lua
    Threads::Threads)

# Scenes resolve Assets/... relative to the working directory, and the
# background image is looked up beside them, so put the binary at the
# repo root rather than in build/.
set_target_properties(raytracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

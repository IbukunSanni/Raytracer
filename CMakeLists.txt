cmake_minimum_required(VERSION 3.16)
project(raytracer CXX C)

set(CMAKE_CXX_STANDARD 17)     # <filesystem> for output-directory creation
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # -std=c++17, not -std=gnu++17, so GCC-only
                                # extensions fail here rather than at the
                                # first attempt to build with MSVC

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------
# Vendored dependencies
#
# This is a CPU renderer: no OpenGL, no windowing, no GUI. It needs
# exactly three third-party pieces, all vendored under third_party/ so
# the repo builds with nothing installed but a compiler and CMake.
#
#   glm     header-only vector/matrix math
#   lodepng PNG encode/decode
#   lua     scene description language
#
# These are marked SYSTEM so their warnings are not ours to fix.
# ---------------------------------------------------------------------

add_library(lodepng STATIC third_party/lodepng/lodepng.cpp)
target_include_directories(lodepng SYSTEM PUBLIC third_party)

file(GLOB LUA_SOURCES third_party/lua-5.3.1/src/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.3.1/src/lua.c    # standalone interpreter
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.3.1/src/luac.c)  # bytecode compiler
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua SYSTEM PUBLIC third_party)

# ---------------------------------------------------------------------
# The renderer
#
# Sources are grouped by concern under src/. Listed explicitly rather
# than globbed: a glob is only re-evaluated when CMake reconfigures, so
# a newly added file silently does not build until someone works out
# why. Explicit lists fail loudly instead.
# ---------------------------------------------------------------------

add_executable(raytracer
    src/main.cpp

    src/core/Image.cpp
    src/core/Log.cpp
    src/core/ToneMap.cpp

    src/math/polyroots.cpp

    src/geometry/Primitive.cpp
    src/geometry/Mesh.cpp
    src/geometry/BVH.cpp

    src/scene/SceneNode.cpp
    src/scene/GeometryNode.cpp
    src/scene/JointNode.cpp
    src/scene/Light.cpp
    src/scene/Material.cpp
    src/scene/PhongMaterial.cpp

    src/render/Renderer.cpp
    src/render/Framebuffer.cpp

    src/lua/scene_lua.cpp
)

# Includes are written relative to src/ (e.g. "geometry/Mesh.hpp"), so
# only src/ and third_party/ are on the path.
target_include_directories(raytracer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(raytracer PRIVATE
    lodepng
    lua
    Threads::Threads)

if(MSVC)
    target_compile_options(raytracer PRIVATE /W3)
    # getenv() is standard C++; MSVC only deprecates it in favour of its own
    # non-portable _dupenv_s.
    target_compile_definitions(raytracer PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(raytracer PRIVATE -Wall)
endif()

# No RUNTIME_OUTPUT_DIRECTORY override: configuring with -B build already
# puts the binary at build/raytracer. Forcing it into the source tree
# meant an out-of-tree build wrote its output back into the repo.

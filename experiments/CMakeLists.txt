# Scratch experiments, built apart from the renderer. This is its own CMake
# project with its own build directory, so nothing here can change how the
# renderer, its tests or the style gate build and run.
#
#     cmake -S experiments -B build/experiments -G "MinGW Makefiles"
#     cmake --build build/experiments
#     build/experiments/stratified_pi
#
# Every .cc in this directory becomes one executable named after the file.
# Helpers shared by experiments go in a .h next to them.

cmake_minimum_required(VERSION 3.16)
project(raytracer_experiments CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)

# Statically link the GCC runtimes on MinGW. Otherwise the binaries resolve
# libstdc++-6.dll against whichever MinGW install sits earliest on PATH, and a
# mismatched one fails at load with STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139).
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++
                     -Wl,-Bstatic,--whole-archive -lwinpthread
                     -Wl,--no-whole-archive)
endif()

# PNG output, for figures.
add_library(lodepng STATIC ../third_party/lodepng/lodepng.cpp)
target_include_directories(lodepng SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)

# What every experiment gets: the renderer's headers, by the same
# src-relative paths the renderer uses ("render/sampling.h"). No renderer .cc
# is compiled in, so only header-only code is reachable: a header whose
# functions live in a .cc fails at link time instead of dragging in the
# renderer's state.
add_library(experiment_support INTERFACE)
target_include_directories(experiment_support INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(experiment_support INTERFACE lodepng Threads::Threads)

# A glob is right here: CONFIGURE_DEPENDS re-checks the directory on every
# build, so a new file is picked up without reconfiguring, and a scratch file
# that is not meant to build can simply be moved out.
file(GLOB EXPERIMENT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

foreach(source IN LISTS EXPERIMENT_SOURCES)
    get_filename_component(name ${source} NAME_WE)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE experiment_support)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W3)
    else()
        target_compile_options(${name} PRIVATE -Wall)
    endif()
endforeach()

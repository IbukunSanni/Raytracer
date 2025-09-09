cmake_minimum_required(VERSION 3.5)
project(CS488Projects)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Include directories
include_directories(
    shared
    shared/include
    shared/gl3w
    shared/imgui
    shared/cs488-framework
)

# Build GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(shared/glfw-3.3)

# Build ImGui
file(GLOB IMGUI_SOURCES 
    shared/imgui/*.cpp
    shared/gl3w/GL/gl3w.c
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC shared/imgui shared/gl3w)

# Build CS488 Framework
file(GLOB FRAMEWORK_SOURCES shared/cs488-framework/*.cpp)
add_library(cs488-framework STATIC ${FRAMEWORK_SOURCES})
target_link_libraries(cs488-framework glfw imgui ${OPENGL_LIBRARIES})

# Build LodePNG
add_library(lodepng STATIC shared/lodepng/lodepng.cpp)

# Build Lua
file(GLOB LUA_SOURCES shared/lua-5.3.1/src/*.c)
list(REMOVE_ITEM LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shared/lua-5.3.1/src/lua.c)
list(REMOVE_ITEM LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shared/lua-5.3.1/src/luac.c)
add_library(lua STATIC ${LUA_SOURCES})

# A0 Project
add_executable(A0 A0/A0.cpp A0/Main.cpp)
target_link_libraries(A0 cs488-framework glfw imgui ${OPENGL_LIBRARIES})
if(WIN32)
    target_link_libraries(A0 opengl32 imm32)
endif()

# A1 Project
file(GLOB A1_SOURCES A1/*.cpp)
add_executable(A1 ${A1_SOURCES})
target_link_libraries(A1 cs488-framework glfw imgui ${OPENGL_LIBRARIES})
if(WIN32)
    target_link_libraries(A1 opengl32 imm32)
endif()

# A2 Project
file(GLOB A2_SOURCES A2/*.cpp)
add_executable(A2 ${A2_SOURCES})
target_link_libraries(A2 cs488-framework glfw imgui ${OPENGL_LIBRARIES})
if(WIN32)
    target_link_libraries(A2 opengl32)
endif()

# A3 Project
file(GLOB A3_SOURCES A3/*.cpp)
add_executable(A3 ${A3_SOURCES})
target_link_libraries(A3 cs488-framework glfw imgui lua ${OPENGL_LIBRARIES})
if(WIN32)
    target_link_libraries(A3 opengl32 imm32)
endif()

# A4 Project
file(GLOB A4_SOURCES A4/*.cpp)
add_executable(A4 ${A4_SOURCES})
target_link_libraries(A4 cs488-framework lodepng lua ${OPENGL_LIBRARIES})
if(WIN32)
    target_link_libraries(A4 opengl32)
endif()
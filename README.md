# Computer Graphics Programming Portfolio

A comprehensive collection of computer graphics applications demonstrating fundamental concepts from 2D transformations to advanced ray tracing. Built using modern OpenGL and C++.

## Projects Overview

### A0: 2D Transformations & GUI
Interactive 2D graphics application featuring:
- Real-time geometric transformations (rotation, scaling, translation)
- ImGui-based user interface with sliders and controls
- Matrix mathematics implementation for 2D operations

**Key Skills:** 2D graphics, matrix transformations, GUI programming

### A1: 3D Maze Navigation
3D first-person maze exploration game with:
- OpenGL 3D rendering pipeline
- Real-time camera movement and collision detection
- Interactive avatar navigation system
- Color customization interface

**Key Skills:** 3D graphics, camera systems, collision detection, OpenGL

### A2: Advanced Camera Controls
Sophisticated 3D viewing system implementing:
- Multiple projection modes (perspective/orthographic)
- Interactive camera manipulation (FOV, near/far planes)
- Real-time viewport transformations
- Advanced viewing matrix calculations

**Key Skills:** 3D mathematics, projection systems, camera controls

### A3: Hierarchical Character Animation
Complex 3D character manipulation system featuring:
- Hierarchical scene graph implementation
- Joint-based character rigging and posing
- Multi-selection node manipulation
- Undo/redo system with state management
- Lua script integration for model loading

**Key Skills:** Scene graphs, hierarchical modeling, animation systems, data structures

### A4: Ray Tracer
High-performance ray tracing renderer with:
- Physically-based lighting calculations
- Anti-aliasing and supersampling
- Bounding volume optimization
- Lua-scripted scene description
- Multi-format output (PNG rendering)

**Key Skills:** Ray tracing algorithms, optimization techniques, lighting models

## Technical Stack

- **Languages:** C++, Lua
- **Graphics:** OpenGL 3.2+, GLM mathematics library
- **UI Framework:** ImGui
- **Build System:** Premake4
- **Platform:** Cross-platform (Linux, macOS, Windows)

## Architecture Highlights

- Modern OpenGL rendering pipeline with shader programs
- Object-oriented design with clean separation of concerns  
- Efficient memory management and resource handling
- Modular project structure for maintainability
- Cross-platform compatibility with consistent build system

---

## Building & Running

### Prerequisites
- OpenGL 3.2+ compatible graphics driver
- C++ compiler with C++11 support
- Premake4 build system

### Quick Start
```bash
# Build all dependencies
premake4 gmake && make

# Run a specific project (example: A4 ray tracer)
cd A4
premake4 gmake && make
./A4 Assets/sample.lua
```

### Dependencies
- **GLFW** - Window management and input handling
- **GLM** - OpenGL mathematics library  
- **Lua** - Scripting engine for scene description
- **ImGui** - Immediate mode GUI framework

### Platform Notes
- **Linux/macOS:** Full support with provided build scripts
- **Windows:** Use `premake4 vs2013` to generate Visual Studio solution files
# CS488 Raytracer - Run Instructions

## Quick Start

All assignments (A0, A1, A2, A3, A4) are successfully built and ready to run!

### Option 1: Master Launcher (Recommended)

**From Root Directory:**
- **Windows**: Double-click `launcher.bat`
- **Linux/Bash**: Run `./launcher.sh`

**From Scripts Directory:**
- **Windows**: Double-click `scripts/run_assignment.bat`
- **Linux/Bash**: Run `./scripts/run_assignment.sh`

Both will show a menu where you can select which assignment to run (0-5).

### Option 2: Individual Assignment Scripts

**Windows (.bat files):**
- `scripts/run_A0.bat` - Run Assignment 0
- `scripts/run_A1.bat` - Run Assignment 1
- `scripts/run_A2.bat` - Run Assignment 2
- `scripts/run_A3.bat` - Run Assignment 3
- `scripts/run_A4.bat` - Run Assignment 4

**Linux/Bash (.sh files):**
- `./scripts/run_A0.sh` - Run Assignment 0
- `./scripts/run_A1.sh` - Run Assignment 1
- `./scripts/run_A2.sh` - Run Assignment 2
- `./scripts/run_A3.sh` - Run Assignment 3
- `./scripts/run_A4.sh` - Run Assignment 4

### Option 3: Manual Execution

If you prefer to run manually:

```bash
# Navigate to the assignment directory
cd A2  # (or A0, A1, A3, A4)

# Set PATH and run executable
PATH="/c/msys64/mingw64/bin:$PATH" ../build/A2.exe
```

## Technical Details

### Fixed Issues:
1. **Missing Runtime Libraries**: Added MinGW runtime libraries to PATH
2. **CMake Configuration**: Fixed missing `imm32` library for all assignments
3. **Working Directory**: Scripts ensure proper working directory for asset loading
4. **A4 Compilation**: Fixed `cbrt` function redefinition error

### Build Information:
- **Build System**: CMake
- **Compiler**: MinGW64 GCC
- **Libraries**: OpenGL, GLFW, ImGui, Lua (for A3/A4), LodePNG (for A4)
- **All executables** are located in the `build/` directory

### Requirements:
- MinGW64 installed at `C:\msys64\mingw64\`
- All required libraries are statically linked

## Troubleshooting

If you encounter issues:

1. **"Command not found" errors**: Ensure MinGW is installed at `C:\msys64\mingw64\`
2. **Missing assets**: Make sure to run from the assignment directory (scripts handle this automatically)
3. **Build issues**: Run `PATH="/c/msys64/mingw64/bin:$PATH" cmake --build build` to rebuild all

## Assignment Descriptions

- **A0**: Basic raytracer setup
- **A1**: Ray-object intersection 
- **A2**: Transformations and hierarchical models
- **A3**: Scene graph with Lua scripting
- **A4**: Advanced raytracer with materials and lighting

All assignments are now properly configured and ready to run!

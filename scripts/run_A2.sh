#!/bin/bash
echo "Running A2 Raytracer..."

# Set PATH to include MinGW libraries
export PATH="/c/msys64/mingw64/bin:$PATH"

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to A2 directory to find Assets
cd "$SCRIPT_DIR/../A2"

# Run A2 executable from the parent build directory
PATH="/c/msys64/mingw64/bin:$PATH" ../build/A2.exe

read -p "Press Enter to continue..."

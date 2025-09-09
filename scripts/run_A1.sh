#!/bin/bash
echo "Running A1 Raytracer..."

# Set PATH to include MinGW libraries
export PATH="/c/msys64/mingw64/bin:$PATH"

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to A1 directory to find Assets
cd "$SCRIPT_DIR/../A1"

# Run A1 executable from the parent build directory
PATH="/c/msys64/mingw64/bin:$PATH" ../build/A1.exe

read -p "Press Enter to continue..."

#!/bin/bash
echo "Running A0 Raytracer..."

# Set PATH to include MinGW libraries
export PATH="/c/msys64/mingw64/bin:$PATH"

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to A0 directory to find Assets
cd "$SCRIPT_DIR/../A0"

# Run A0 executable from the parent build directory
PATH="/c/msys64/mingw64/bin:$PATH" ../build/A0.exe

read -p "Press Enter to continue..."

#!/bin/bash
echo "Running A3 Raytracer..."

# Set PATH to include MinGW libraries
export PATH="/c/msys64/mingw64/bin:$PATH"

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to A3 directory to find Assets
cd "$SCRIPT_DIR/../A3"

# Run A3 executable from the parent build directory with default scene
PATH="/c/msys64/mingw64/bin:$PATH" ../build/A3.exe Assets/puppet.lua

read -p "Press Enter to continue..."

#!/bin/bash

while true; do
    echo "============================================"
    echo "       CS488 Raytracer Assignment Launcher"
    echo "============================================"
    echo
    echo "Available assignments:"
    echo "  0. A0 - Assignment 0"
    echo "  1. A1 - Assignment 1"  
    echo "  2. A2 - Assignment 2"
    echo "  3. A3 - Assignment 3"
    echo "  4. A4 - Assignment 4"
    echo "  5. Exit"
    echo
    read -p "Select assignment (0-5): " choice

    case $choice in
        0)
            echo "Running A0..."
            bash "$(dirname "$0")/run_A0.sh"
            ;;
        1)
            echo "Running A1..."
            bash "$(dirname "$0")/run_A1.sh"
            ;;
        2)
            echo "Running A2..."
            bash "$(dirname "$0")/run_A2.sh"
            ;;
        3)
            echo "Running A3..."
            bash "$(dirname "$0")/run_A3.sh"
            ;;
        4)
            echo "Running A4..."
            bash "$(dirname "$0")/run_A4.sh"
            ;;
        5)
            echo "Goodbye!"
            exit 0
            ;;
        *)
            echo "Invalid choice. Please select 0-5."
            read -p "Press Enter to continue..."
            ;;
    esac
done

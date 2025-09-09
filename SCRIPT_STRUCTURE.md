# CS488 Raytracer - Script Structure

## Directory Organization

```
Raytracer/
├── launcher.bat              # Main Windows launcher (calls scripts/run_assignment.bat)
├── launcher.sh               # Main Linux launcher (calls scripts/run_assignment.sh)
├── RUN_INSTRUCTIONS.md       # Detailed usage instructions
├── SCRIPT_STRUCTURE.md       # This file
├── build/                    # Built executables
│   ├── A0.exe
│   ├── A1.exe
│   ├── A2.exe
│   ├── A3.exe
│   └── A4.exe
├── scripts/                  # All run scripts organized here
│   ├── run_assignment.bat    # Interactive menu launcher (Windows)
│   ├── run_assignment.sh     # Interactive menu launcher (Linux)
│   ├── run_A0.bat           # Individual assignment launchers
│   ├── run_A0.sh
│   ├── run_A1.bat
│   ├── run_A1.sh
│   ├── run_A2.bat
│   ├── run_A2.sh
│   ├── run_A3.bat
│   ├── run_A3.sh
│   ├── run_A4.bat
│   └── run_A4.sh
├── A0/                      # Assignment directories with assets
├── A1/
├── A2/
├── A3/
└── A4/
```

## Quick Usage

**Easiest Method:**
- Windows: Double-click `launcher.bat`
- Linux: Run `./launcher.sh`

**Alternative:**
- Windows: Double-click `scripts/run_assignment.bat`  
- Linux: Run `./scripts/run_assignment.sh`

Both show an interactive menu to select assignments 0-5.

## How the Scripts Work

1. **Root Launchers** (`launcher.bat`/`launcher.sh`): Simple convenience scripts that call the main launchers in the scripts folder.

2. **Main Launchers** (`scripts/run_assignment.*`): Interactive menu system that calls individual assignment scripts.

3. **Individual Scripts** (`scripts/run_A*.{bat,sh}`): Each script:
   - Sets up MinGW PATH for runtime libraries
   - Changes to the appropriate assignment directory (A0, A1, etc.)
   - Runs the executable from `../build/A*.exe`
   - Handles proper asset loading by setting correct working directory

All scripts are self-contained and work regardless of where they're called from, thanks to dynamic path resolution using script location detection.

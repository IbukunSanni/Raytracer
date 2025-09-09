@echo off
echo Running A3 Raytracer...

REM Set PATH to include MinGW libraries
set PATH=C:\msys64\mingw64\bin;%PATH%

REM Change to A3 directory to find Assets
cd /d "%~dp0..\A3"

REM Run A3 executable from the parent build directory with default scene
PATH=C:\msys64\mingw64\bin;%PATH% ..\build\A3.exe Assets/simpleScene.lua

pause

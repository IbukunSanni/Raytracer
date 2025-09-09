@echo off
echo Running A1 Raytracer...

REM Set PATH to include MinGW libraries
set PATH=C:\msys64\mingw64\bin;%PATH%

REM Change to A1 directory to find Assets
cd /d "%~dp0..\A1"

REM Run A1 executable from the parent build directory
PATH=C:\msys64\mingw64\bin;%PATH% ..\build\A1.exe

pause

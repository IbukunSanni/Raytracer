@echo off
echo Running A2 Raytracer...

REM Set PATH to include MinGW libraries
set PATH=C:\msys64\mingw64\bin;%PATH%

REM Change to A2 directory to find Assets
cd /d "%~dp0..\A2"

REM Run A2 executable from the parent build directory
PATH=C:\msys64\mingw64\bin;%PATH% ..\build\A2.exe

pause

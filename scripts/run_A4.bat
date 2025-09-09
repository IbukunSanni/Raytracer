@echo off
echo Running A4 Raytracer...

REM Set PATH to include MinGW libraries
set PATH=C:\msys64\mingw64\bin;%PATH%

REM Change to A4 directory to find Assets
cd /d "%~dp0..\A4"

REM Run A4 executable from the parent build directory
PATH=C:\msys64\mingw64\bin;%PATH% ..\build\A4.exe

pause

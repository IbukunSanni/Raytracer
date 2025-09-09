@echo off
echo Running A0 Raytracer...

REM Set PATH to include MinGW libraries
set PATH=C:\msys64\mingw64\bin;%PATH%

REM Change to A0 directory to find Assets
cd /d "%~dp0..\A0"

REM Run A0 executable from the parent build directory
PATH=C:\msys64\mingw64\bin;%PATH% ..\build\A0.exe

pause

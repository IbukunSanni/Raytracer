@echo off
echo CS488 Graphics Projects - Quick Run Script
echo ==========================================

REM Build the static libraries first
echo Building static libraries...
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" BuildStaticLibs.sln /p:Configuration=Release

if %ERRORLEVEL% NEQ 0 (
    echo Failed to build static libraries. Opening Visual Studio...
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" BuildStaticLibs.sln
    pause
    exit /b 1
)

echo Static libraries built successfully!
echo.

REM Build and run each project
echo Building A0 (2D Transformations)...
cd A0
..\premake5.exe vs2022
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" CS488-Projects.sln /p:Configuration=Release

if %ERRORLEVEL% EQU 0 (
    echo Running A0...
    start A0.exe
) else (
    echo A0 build failed. Opening in Visual Studio...
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" CS488-Projects.sln
)

cd ..

echo.
echo Building A1 (3D Maze)...
cd A1
..\premake5.exe vs2022
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" CS488-Projects.sln /p:Configuration=Release

if %ERRORLEVEL% EQU 0 (
    echo Running A1...
    start A1.exe
) else (
    echo A1 build failed. Opening in Visual Studio...
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" CS488-Projects.sln
)

cd ..

echo.
echo Building A2 (Camera Controls)...
cd A2
..\premake5.exe vs2022
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" CS488-Projects.sln /p:Configuration=Release

if %ERRORLEVEL% EQU 0 (
    echo Running A2...
    start A2.exe
) else (
    echo A2 build failed. Opening in Visual Studio...
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" CS488-Projects.sln
)

cd ..

echo.
echo Building A3 (Character Animation)...
cd A3
..\premake5.exe vs2022
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" CS488-Projects.sln /p:Configuration=Release

if %ERRORLEVEL% EQU 0 (
    echo Running A3...
    start A3.exe Assets/puppet.lua
) else (
    echo A3 build failed. Opening in Visual Studio...
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" CS488-Projects.sln
)

cd ..

echo.
echo Building A4 (Ray Tracer)...
cd A4
..\premake5.exe vs2022
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" CS488-Projects.sln /p:Configuration=Release

if %ERRORLEVEL% EQU 0 (
    echo Running A4 Ray Tracer...
    A4.exe Assets/sample.lua
    echo Ray traced image saved as sample.png
    start sample.png
) else (
    echo A4 build failed. Opening in Visual Studio...
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" CS488-Projects.sln
)

cd ..

echo.
echo All projects processed! Check the windows that opened.
pause
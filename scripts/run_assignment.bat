@echo off
:start
echo ============================================
echo        CS488 Raytracer Assignment Launcher
echo ============================================
echo.
echo Available assignments:
echo   0. A0 - Assignment 0
echo   1. A1 - Assignment 1  
echo   2. A2 - Assignment 2
echo   3. A3 - Assignment 3
echo   4. A4 - Assignment 4
echo   5. Exit
echo.
set /p choice="Select assignment (0-5): "

if "%choice%"=="0" (
    echo Running A0...
    call "%~dp0run_A0.bat"
) else if "%choice%"=="1" (
    echo Running A1...
    call "%~dp0run_A1.bat"
) else if "%choice%"=="2" (
    echo Running A2...
    call "%~dp0run_A2.bat"
) else if "%choice%"=="3" (
    echo Running A3...
    call "%~dp0run_A3.bat"
) else if "%choice%"=="4" (
    echo Running A4...
    call "%~dp0run_A4.bat"
) else if "%choice%"=="5" (
    echo Goodbye!
    exit /b
) else (
    echo Invalid choice. Please select 0-5.
    pause
    goto start
)

goto start

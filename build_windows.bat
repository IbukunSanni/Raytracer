@echo off
echo Setting up Windows build environment...

REM Download premake4 if not present
if not exist premake4.exe (
    echo Downloading premake4...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/premake/premake-4.x/releases/download/v4.4-beta5/premake-4.4-beta5-windows.zip' -OutFile 'premake4.zip'"
    powershell -Command "Expand-Archive -Path 'premake4.zip' -DestinationPath '.'"
    del premake4.zip
)

REM Try to build with Visual Studio
echo Generating Visual Studio project files...
premake4.exe vs2013

if exist CS488-Projects.sln (
    echo Opening Visual Studio solution...
    start CS488-Projects.sln
) else (
    echo Failed to generate Visual Studio files. Trying alternative approach...
    
    REM Try MinGW approach
    echo Setting up MinGW build...
    set PATH=C:\msys64\mingw64\bin;%PATH%
    premake4.exe gmake
    
    if exist Makefile (
        echo Building with MinGW...
        mingw32-make
    ) else (
        echo Build system setup failed. Please install Visual Studio Community 2022.
    )
)

pause
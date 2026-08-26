@echo off
echo ================================================
echo  WetEQ VST3 Plugin - Build Script
echo ================================================
echo.

REM Clean build directory for fresh build
if exist "WetEQ\build" (
    echo Cleaning previous build...
    rmdir /S /Q WetEQ\build
)

echo Creating fresh build directory...
mkdir WetEQ\build

cd WetEQ\build

echo.
echo Step 1: Configuring CMake...
echo ================================================
cmake .. -G "Visual Studio 17 2022" -A x64 -DSMTG_CREATE_PLUGIN_LINK=0
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    cd ..\..
    exit /b 1
)

echo.
echo Step 2: Building Release configuration...
echo ================================================
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed!
    cd ..\..
    exit /b 1
)

echo.
echo ================================================
echo  Build successful!
echo ================================================
echo.
echo Plugin location: WetEQ\build\VST3\Release\WetEQ.vst3
echo.
echo Run install.bat to copy the plugin to your VST3 folder
echo.

cd ..\..
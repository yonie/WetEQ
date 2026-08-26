@echo off
REM Build and run eqtest - measures EQEngine with no plugin and no host.
REM Run from the plugin repo root:  tools\build-eqtest.bat

set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" (
    echo ERROR: vcvars64.bat not found at "%VCVARS%"
    exit /b 1
)
call "%VCVARS%" >nul

if not exist build-tools mkdir build-tools

cl /nologo /EHsc /O2 /std:c++17 /I WetEQ\source ^
   /Fe:build-tools\eqtest.exe /Fo:build-tools\ ^
   tools\eqtest.cpp WetEQ\source\eqengine.cpp
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

echo.
build-tools\eqtest.exe
exit /b %errorlevel%

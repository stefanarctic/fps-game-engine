@echo off
REM ---------------------------------------------------------------------------
REM  Builds Forge Engine + the FPS game with MSVC, CMake and Ninja.
REM  Usage:  build.bat [Release|Debug|RelWithDebInfo]   (default: Release)
REM  Output: build\<config>\bin\FPSGame.exe
REM ---------------------------------------------------------------------------
setlocal enableextensions

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
    echo [error] vswhere.exe not found - install Visual Studio 2019/2022 with the C++ workload.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (
    echo [error] No Visual Studio installation with MSVC x64 tools found.
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
where cl >nul 2>nul
if errorlevel 1 (
    echo [error] The MSVC compiler ^(cl.exe^) is not available after vcvars64.bat.
    exit /b 1
)

REM Prefer the CMake/Ninja that ship with Visual Studio when not on PATH.
set PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%

set ROOT=%~dp0
set BUILD_DIR=%ROOT%build\%CONFIG%

REM Force MSVC so another compiler on PATH (e.g. an old MinGW) is never picked up.
cmake -S "%ROOT%." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build "%BUILD_DIR%" || exit /b 1

echo.
echo Build succeeded: %BUILD_DIR%\bin\FPSGame.exe
endlocal

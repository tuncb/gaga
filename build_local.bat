@echo off
setlocal

set "BUILD_TYPE=Debug"
set "BUILD_DIR=build-debug"

if /I "%~1"=="debug" (
    set "BUILD_TYPE=Debug"
    set "BUILD_DIR=build-debug"
    shift
) else if /I "%~1"=="release" (
    set "BUILD_TYPE=Release"
    set "BUILD_DIR=build-release"
    shift
)

pushd "%~dp0"

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    popd
    exit /b %errorlevel%
)

cmake -S . -B "%BUILD_DIR%" --fresh -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    popd
    exit /b %errorlevel%
)

cmake --build "%BUILD_DIR%"
set "RESULT=%errorlevel%"

popd
exit /b %RESULT%

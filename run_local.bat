@echo off
setlocal EnableDelayedExpansion

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

call "%~dp0build_local.bat" %BUILD_TYPE%
if errorlevel 1 (
    set "RESULT=%errorlevel%"
    popd
    exit /b !RESULT!
)

set "APP_ARGS="
:collect_args
if "%~1"=="" goto run_app
set "APP_ARGS=!APP_ARGS! "%~1""
shift
goto collect_args

:run_app
"%~dp0%BUILD_DIR%\gaga.exe"!APP_ARGS!
set "RESULT=%errorlevel%"

popd
exit /b %RESULT%

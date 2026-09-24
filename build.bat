@echo off
rem Usage: build.bat [debug^|release^|release-clang] [--no-run]
setlocal

cd /d "%~dp0"

set CONFIG=debug
set RUN=1

:parse
if "%~1"=="" goto done
if /i "%~1"=="debug" (
    set CONFIG=debug
) else if /i "%~1"=="release" (
    set CONFIG=release
) else if /i "%~1"=="release-clang" (
    set CONFIG=release-clang
) else if /i "%~1"=="--no-run" (
    set RUN=0
) else (
    goto badarg
)
shift
goto parse

:badarg
if /i "%~1"=="-h"     goto usage
if /i "%~1"=="--help" goto usage
echo Unknown argument: %~1
goto usage_error
:done

set PRESET=%CONFIG%-windows
if "%CONFIG%"=="release-clang" set PRESET=release-windows-clang

rem Keep in sync with the windows presets' binaryDir.
set OUTDIR=out\build\debug
if not "%CONFIG%"=="debug" set OUTDIR=out\build\release

cmake --preset=%PRESET% || exit /b 1
cmake --build --preset=%PRESET% || exit /b 1

if "%RUN%"=="1" start "path-tracer" cmd /k ".\%OUTDIR%\path-tracer.exe"
exit /b 0

:usage
echo Usage: build.bat [debug^|release^|release-clang] [--no-run]
exit /b 0

:usage_error
echo Usage: build.bat [debug^|release^|release-clang] [--no-run]
exit /b 1

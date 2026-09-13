@echo off
setlocal
cd /d "%~dp0"

rem Default Debug; pass "release" for Release
set "BUILD_TYPE=Debug"
if /I "%~1"=="release" set "BUILD_TYPE=Release"
if /I "%~1"=="debug" set "BUILD_TYPE=Debug"

set "BUILD_DIR=build"
if "%BUILD_TYPE%"=="Release" set "BUILD_DIR=build_release"

echo === Crest Build (%BUILD_TYPE%) ===
echo.

cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_CXX_COMPILER=clang++
if errorlevel 1 (
    echo.
    echo Configure failed.
    exit /b 1
)

cmake --build "%BUILD_DIR%"
if errorlevel 1 (
    echo.
    echo Build failed.
    exit /b 1
)

rem Copy exe + runtime DLL to project root.
rem POST_BUILD only runs when the target is actually rebuilt; on no-op
rem builds ("ninja: no work to do") it is skipped, so copy here explicitly.
copy /Y "%BUILD_DIR%\crest.exe" crest.exe >nul
if errorlevel 1 (
    echo.
    echo Copy crest.exe to project root failed.
    exit /b 1
)
copy /Y "bin\LLVM-C.dll" LLVM-C.dll >nul
if errorlevel 1 (
    echo.
    echo Copy LLVM-C.dll to project root failed.
    exit /b 1
)

echo.
echo === Done: crest.exe copied to project root ===

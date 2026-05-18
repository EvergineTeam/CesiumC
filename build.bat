@echo off
REM Build script for CesiumNativeC.dll
REM Prerequisites: CMake 3.15+, Visual Studio 2022, vcpkg

setlocal

set BUILD_DIR=%~dp0build
set VCPKG_ROOT=%VCPKG_ROOT%
set GENERATOR=Visual Studio 18 2026

if "%CMAKE_GENERATOR%" neq "" (
    set GENERATOR=%CMAKE_GENERATOR%
)

if "%VCPKG_ROOT%"=="" (
    echo ERROR: VCPKG_ROOT environment variable is not set.
    echo Please install vcpkg and set VCPKG_ROOT to its location.
    exit /b 1
)

if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo ===== Clearing stale CMake cache =====
    del /f /q "%BUILD_DIR%\CMakeCache.txt" >nul 2>nul
)
if exist "%BUILD_DIR%\CMakeFiles" (
    rmdir /s /q "%BUILD_DIR%\CMakeFiles"
)

echo ===== Configuring CesiumNativeC with %GENERATOR% =====
cmake -S "%~dp0." -B "%BUILD_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DCMAKE_BUILD_TYPE=Release ^
    -G "%GENERATOR%" -A x64

if %ERRORLEVEL% neq 0 (
    if "%GENERATOR%"=="Visual Studio 18 2026" (
        echo ===== Retrying with Visual Studio 17 2022 =====
        cmake -S "%~dp0." -B "%BUILD_DIR%" ^
            -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
            -DVCPKG_TARGET_TRIPLET=x64-windows ^
            -DCMAKE_BUILD_TYPE=Release ^
            -G "Visual Studio 17 2022" -A x64
    )
)

if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

echo ===== Building CesiumNativeC =====
cmake --build "%BUILD_DIR%" --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed.
    exit /b 1
)

echo ===== Build succeeded =====
echo Output: %BUILD_DIR%\bin\Release\CesiumNativeC.dll

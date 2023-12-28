@echo off
setlocal
set BUILD_TYPE=%2
set UTILS_BUILD_MODE=%1
set TARGET=%3
if "%BUILD_TYPE%" == "" (
    set BUILD_TYPE=Debug
)
if "%UTILS_BUILD_MODE%" == "" (
    set UTILS_BUILD_MODE=shared
)

set TRUE_FALSE=false
if "%UTILS_BUILD_MODE%" == "shared" (
     set TRUE_FALSE=true
)
if "%UTILS_BUILD_MODE%" == "static" (
     set TRUE_FALSE=true
)
if "%UTILS_BUILD_MODE%" == "wasm-rt" (
     set TRUE_FALSE=true
)
if "%UTILS_BUILD_MODE%" == "wasm-em" (
     set TRUE_FALSE=true
)
if "%UTILS_BUILD_MODE%" == "gcc-shared" (
     set TRUE_FALSE=true
)
if "%UTILS_BUILD_MODE%" == "gcc-static" (
     set TRUE_FALSE=true
)

if "%TRUE_FALSE%"=="false" (
    echo "Invalid UTILS_BUILD_MODE: %UTILS_BUILD_MODE%"
    exit /b
)

set CXX_COMPILER=clang++
set C_COMPILER=clang
set OUTPUT_DIR=built/%UTILS_BUILD_MODE%/%BUILD_TYPE%

if "%UTILS_BUILD_MODE%" == "gcc-static" (
    set CXX_COMPILER=g++
    set C_COMPILER=gcc
    set UTILS_BUILD_MODE=static
)

if "%UTILS_BUILD_MODE%" == "gcc-shared" (
    set CXX_COMPILER=g++
    set C_COMPILER=gcc
    set UTILS_BUILD_MODE=shared
)

rem override CXX_COMPILER and C_COMPILER if specified with UTILS_CXX_COMPILER and UTILS_C_COMPILER
if not "%UTILS_CXX_COMPILER%" == "" (
    set CXX_COMPILER=%UTILS_CXX_COMPILER%
)
if not "%UTILS_C_COMPILER%" == "" (
    set C_COMPILER=%UTILS_C_COMPILER%
)


set LLVM_DIR=D:\llvm-project\%BUILD_TYPE%
set INSTALL_PREFIX=%CD%
rem -D CMAKE_CXX_CLANG_TIDY=clang-tidy;-header-filter=src/include;--checks=-*
rem if "%CLANG%"=="true" (
if "%UTILS_BUILD_MODE%" == "wasm-rt" (
    rem WASI_SDK_PATH must be enabled on this context
    cmake -D CMAKE_TOOLCHAIN_FILE=%WASI_SDK_PREFIX%/share/cmake/wasi-sdk-pthread.cmake -D WASI_SDK_PREFIX=%WASI_SDK_PREFIX%  -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -D CMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% -D UTILS_BUILD_MODE=static -S . -B %OUTPUT_DIR%
) else if "%UTILS_BUILD_MODE%" == "wasm-em" (
    rem emcmake must be enabled on this context
    call emcmake cmake -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -D CMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% -D UTILS_BUILD_MODE=static -S . -B %OUTPUT_DIR%
) else (
    cmake -D CMAKE_CXX_COMPILER=%CXX_COMPILER% -D CMAKE_C_COMPILER=%C_COMPILER% -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -D CMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% -D UTILS_BUILD_MODE=%UTILS_BUILD_MODE% -S . -B %OUTPUT_DIR%
)
rem ) 
rem else (
rem     call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
rem     call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
rem     cmake -D CMAKE_CXX_COMPILER=cl -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% .
rem )
ninja -C %OUTPUT_DIR% %TARGET%
if not %ERRORLEVEL% == 0 (
    exit /b
)
ninja -C %OUTPUT_DIR% install

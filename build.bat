@echo off
setlocal
set CLANG=true
set BUILD_TYPE=%2
set UTILS_BUILD_TYPE=%1
set TARGET=%3
if "%BUILD_TYPE%" == "" (
    set BUILD_TYPE=Debug
)
if "%UTILS_BUILD_TYPE%" == "" (
    set UTILS_BUILD_TYPE=shared
)


set LLVM_DIR=D:\llvm-project\%BUILD_TYPE%
set INSTALL_PREFIX=%CD%
rem -D CMAKE_CXX_CLANG_TIDY=clang-tidy;-header-filter=src/include;--checks=-*
if "%CLANG%"=="true" (
    cmake -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_C_COMPILER=clang -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% -D CMAKE_INSTALL_PREFIX=%INSTALL_PREFIX% -D UTILS_BUILD_TYPE=%UTILS_BUILD_TYPE% -S . -B built/%UTILS_BUILD_TYPE%/%BUILD_TYPE%
) else (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    cmake -D CMAKE_CXX_COMPILER=cl -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% .
)
ninja -C built/%UTILS_BUILD_TYPE%/%BUILD_TYPE% %TARGET%
ninja -C built/%UTILS_BUILD_TYPE%/%BUILD_TYPE% install

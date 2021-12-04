@echo off
setlocal
set BUILTIN_BINARY=1
set CLANG=true
set BUILD_TYPE=Debug
if "%CLANG%"=="true" (
    cmake -D CMAKE_CXX_COMPILER=clang++ -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% .
) else (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    cmake -D CMAKE_CXX_COMPILER=cl -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% .
)
ninja

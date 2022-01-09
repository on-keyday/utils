@echo off
setlocal
set CLANG=true
set BUILD_TYPE=Debug
rem -D CMAKE_CXX_CLANG_TIDY=clang-tidy;-header-filter=src/include;--checks=-*
if "%CLANG%"=="true" (
    cmake -D CMAKE_CXX_COMPILER=clang++ -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% .
) else (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    cmake -D CMAKE_CXX_COMPILER=cl -G Ninja -D CMAKE_BUILD_TYPE=%BUILD_TYPE% .
)
ninja

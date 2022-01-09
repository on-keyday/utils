# utils
this library is reimplementation of my made utility library "commonlib", 
to remove needless dependency, dirty hack, and to improve portability and visibility. (now imcompleted)

library was tested easily with code on src/test. (may be not enough coverage)

# how to build
1. install cmake (https://cmake.org/), ninja (https://ninja-build.org/) and clang (https://clang.llvm.org/) on your platform.
2. if you want to build `libutils` as shared library or dll, edit CMakeLists.txt option UTILS_BUILD_SHARED_LIBS. also define macro UTILS_AS_DLL if platform is windows
3. run `. build` (on linux) or `build.bat` (on windows)
4. `libutils` will be built on `lib/`, test program will be built on `test/`, tool will be built on `tool/`

# std version
C++20

# License

MIT license

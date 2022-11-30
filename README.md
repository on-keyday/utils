# utils
this library is developing only for my fun and study
this library has no dependency but has OpenSSL for deprecated library.
you will be able to build from only these codes when deprecated is removed.
I don't provide guarantee for your problem if you use these code.


# what including (2022/11/30)
+ async - fiber/ucontext based coroutine
+ bin_fmt - binray format (unusable)
+ bnf - bnf parser (stop developing)
+ cmdline - command line parser (stable)
+ core - Sequencer class (base of parsers) (stable)
+ deprecated - dust box
+ dnet - networking library (active developing)
  + socket wrapper
  + tls wrapper (OpenSSL/BoringSSL)
    + this wrapper can be built without OpenSSL/BroginSSL headers
  + http
  + http2 (unstable)
  + websocket
  + server util
  + stun (unstable)
  + QUIC (active developing)
+ endian - endian util (stable)
+ escape - string escapes (stable)
+ file - file util (stable)
  + huffman.h is unstable
+ fmt - format string (not developed)
+ helper - implementation helper library
  + I think this is actual 'utility' library
+ iface - golang like interface (deprecated but because code using this is stable,remaining)
+ json - JSON parser/renderer (stable)
+ minilang - lambda combination based mini programing language parser (stable/developing)
+ net_util - network utility (base64/punycode/hpack/URI/URLencoding/cookie/date/sha1/ipaddress etc...) (stable)
+ number - number parser/renderer (stable)
+ platform - plarform dependent (deprecated but because code using this is too many,remaining)
+ testutil - test utility (stop developing/stable)
+ thread - thread utility (channel) (stable)
+ utf - utf converter (stable)
+ wrap - wrapper (argv/cout/cin/pair_iter) (stable)

no documentation is here so you should read source code (yes I'm not kind)
also no code uniformity among directories (yes, may be among codes in same directory too)

# how to build
1. install cmake (https://cmake.org/), ninja (https://ninja-build.org/) and clang (https://clang.llvm.org/) on your platform.
2. if you want to build `libutils` as shared library or dll, edit CMakeLists.txt option UTILS_BUILD_SHARED_LIBS. also define macro UTILS_AS_DLL if platform is windows
3. run `. build` (on linux) or `build.bat` (on windows)
4. `libutils` will be built on `lib/`, test program will be built on `test/`, tool will be built on `tool/`

# std version
C++20

# License

MIT license

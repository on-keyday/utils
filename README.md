# utils
this library is developing only for my fun and study.
this library need no dependency except standard library to build.
I don't provide guarantee for your problem if you use code in this repository.

# warning
Because I didn't know about OpenSSL license, this library was maybe at license violation. (commits before 2023/2/14)
Now I have added these two note for src/include/fnet/tls/tls.h:
+ This product includes cryptographic software written by Eric Young (eay@cryptsoft.com)
+ This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)

these note is based on OpenSSL license's advertising clause.
But, this library doesn't contains any source code or binary written by OpenSSL.
Somehow the note above sounds like a joke.


# what is included (2023/9/14)
+ binary - binary reader/writer/endian util/bit flags helper/IEEE float (stable)
+ cmdline - command line parser (stable)
+ code - source code writer/error format utility (indented writer) (stable)
+ comb2 - parser combinator library (stable)
+ core - Sequencer class (base of parsers)/define byte type (stable)
+ coro  - fiber/ucontext based coroutine
+ env - environment variable (stable)
+ fnet - networking library (active developing)
  + socket wrapper
    + because my main developing platform is windows, epoll developing is put off
    + IOCP (stable)
    + epoll (can build, behaviour is not enough tested)  
  + getaddrinfo wrapper
  + tls wrapper (OpenSSL/BoringSSL)
    + this wrapper can be built without OpenSSL/BroginSSL headers
  + http
  + http2 (unstable)
  + dns (tiny)
  + websocket
  + server util
  + stun (unstable)
  + QUIC (active developing) (see also src/test/test_fnetquic_multi_thread.cpp)
    + connection establishment and idle timeout.
    + immediate close
    + handshake timeout (avoid dead lock)
    + have path MTU discovery
    + have Retry handling 
    + have stream
  + util - network utility (base64/punycode/hpack/qpack/URI/URLencoding/cookie/date/sha1/ipaddress etc...) (stable)
+ escape - string escapes (stable)
+ file - file util (stable)
  + gzip - gzip decoder (no encoder)
+ helper - implementation helper library
  + I think this is actual 'utility' library
+ json - JSON parser/renderer (stable)
+ langc - too tiny c lang compiler (not full feature supported)
+ math - mathematical utility (this is toy level)
+ number - number parser/renderer (stable)
+ platform/windows - windows platform dependent (deprecated but because code using this is too many,remaining)
+ reflect - reflection `like` library (not complete reflection)
+ strutil - string treatment utility
+ testutil - test utility (stop developing/stable)
+ thread - thread utility (channel) (stable)
+  unicode
   + data - unicodedata.txt treatment  
   + utf - utf converter (stable)
+ view - view helper (access like array, has operator[] and size())
  + iovec,expand_iovec - byte vector like std::string_view and std::string (reinventing the wheel)
+ wasm - WebAssembly binary parser 
+ wrap - wrapper (argv/cout/cin/pair_iter) (stable)
+ yaml - YAML parser (incomplete)

no documentation is here so you should read source code (yes I'm not kind)
also no code uniformity may be among directories (yes, may be among codes in same directory too)
API compatibility is often broken among each large commits. (yes, I throw stability away)

# how to build
1. install cmake (https://cmake.org/), ninja (https://ninja-build.org/) and clang (https://clang.llvm.org/) on your platform.
2. run `. build` (on linux) or `build.bat` (on windows)
3. `libutils`,`libfnet`,`libfnetserv`, `libcoro` will be built on `lib/`, test program (it's too presumptuous to explain as test) will be built on `test/`, tool will be built on `tool/`

# std version
C++20

# License

MIT license

cmake_minimum_required(VERSION 3.16)
project(utils)

if(WIN32)
include_directories("D:/OpenSSL/build-x64-debug/include")
link_directories("D:/OpenSSL/build-x64-debug/lib")
if(MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
endif()
else()
# find_package(OPENSSL REQUIRED)
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
link_directories("/lib/llvm-12/lib")
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
set(CMAKE_CXX_FLAGS "-nostdinc++ -nodefaultlibs -isystem /lib/llvm-12/include/c++/v1 -fuse-ld=lld")
set(CMAKE_EXE_LINKER_FLAGS  "-lc++ -lc++abi -lm -lc -lgcc_s -lgcc")
endif()
endif()


set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")
add_executable(core "src/test/core/test_core.cpp")
add_executable(utfconvert "src/test/utf/test_utfconvert.cpp")
add_executable(fallback "src/test/utf/test_fallback_sequence.cpp")
add_executable(utfview "src/test/utf/test_utfview.cpp")
add_executable(utf_operator "src/test/utf/test_utf_operator.cpp")
add_executable(endian "src/test/endian/test_endian.cpp")
add_executable(fileview "src/test/file/test_file_view.cpp")
add_executable(cout "src/test/wrap/test_cout.cpp")
add_executable(channel "src/test/thread/test_channel.cpp")
add_executable(predefined "src/test/tokenize/test_predefined.cpp")
add_executable(tokenizer "src/test/tokenize/test_tokenizer.cpp")
add_executable(make_parser "src/test/syntax/test_make_parser_clang_bug.cpp")
add_executable(syntaxc "src/test/syntax/test_syntaxc.cpp")
add_executable(number "src/test/number/test_number.cpp")
add_executable(cmdparse "src/test/cmdline/test_cmdparse.cpp")
add_executable(netcore "src/test/net/test_netcore.cpp")
add_executable(urlparse "src/test/net/test_urlparse.cpp")
add_executable(base64 "src/test/net/test_base64.cpp")
add_executable(urlencode "src/test/net/test_urlencode.cpp")
add_executable(sha1 "src/test/net/test_sha1.cpp")
add_executable(to_string "src/test/number/test_to_string.cpp")
add_executable(punycode "src/test/net/test_punycode.cpp")
add_executable(http1 "src/test/net/test_http1.cpp")
if(WIN32)
add_executable(io_completion_port "src/test/platform/windows/test_io_completion_port.cpp")
endif()
add_executable(cookie "src/test/net/test_cookie.cpp")
add_executable(strutil "src/test/helper/test_strutil.cpp")
add_executable(dispatcher "src/test/syntax/test_dispatcher.cpp")
add_executable(tree "src/test/syntax/test_tree.cpp")
add_executable(escape "src/test/escape/test_escape.cpp")
add_executable(json "src/test/json/test_json.cpp")
add_executable(cin "src/test/wrap/test_cin.cpp")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")
add_executable(ifacegen "src/tool/ifacegen/interface_gen.cpp"
                        "src/tool/ifacegen/read_interface.cpp"
                        "src/tool/ifacegen/generate_interface.cpp")
add_executable(binred "src/tool/binred/binred.cpp"
                      "src/tool/binred/read_fmt.cpp"
                      "src/tool/binred/generate_cpp.cpp")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "lib")
if(WIN32)
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()


option(UTILS_BUILD_SHARED_LIBS "build utils as a shared library" ON)

if(UTILS_BUILD_SHARED_LIBS)
add_library(utils SHARED)
add_compile_options(utils -D UTILS_AS_DLL)
else()
add_library(utils STATIC)
endif()

target_sources(utils
PRIVATE
"src/lib/file/platform.cpp"
"src/lib/file/file_view.cpp"
"src/lib/wrap/argv.cpp"
"src/lib/wrap/cout.cpp"
"src/lib/syntax/default_syntaxc.cpp"
"src/lib/net/core/init_net.cpp"
"src/lib/net/dns/dns.cpp"
"src/lib/net/tcp/tcp.cpp"
"src/lib/net/ssl/ssl_common.cpp"
"src/lib/net/ssl/ssl_conn.cpp"
"src/lib/net/http/http1.cpp"
"src/lib/platform/windows/io_completion_port.cpp" 
"src/lib/json/json_object.cpp"
"src/lib/wrap/iocommon.cpp"
"src/lib/wrap/cin.cpp"
)


target_link_libraries(fileview utils)
target_link_libraries(cout utils)
target_link_libraries(channel utils Threads::Threads)
target_link_libraries(tokenizer utils)
target_link_libraries(make_parser utils)
target_link_libraries(syntaxc utils)
target_link_libraries(cmdparse utils)
target_link_libraries(netcore utils)
target_link_libraries(http1 utils Threads::Threads)
if(WIN32)
target_link_libraries(io_completion_port utils)
endif()
target_link_libraries(dispatcher utils)
target_link_libraries(tree utils)
target_link_libraries(json utils)
target_link_libraries(cin utils)

target_link_libraries(ifacegen utils)
target_link_libraries(binred utils)

if(WIN32)
target_link_libraries(netcore libssl libcrypto ws2_32)
target_link_libraries(http1 libssl libcrypto ws2_32)
target_link_libraries(io_completion_port libssl libcrypto ws2_32)
if(UTILS_BUILD_SHARED_LIBS)
target_link_libraries(utils libssl libcrypto ws2_32)
endif()
else()
target_link_libraries(netcore libssl.so libcrypto.so Threads::Threads)
target_link_libraries(http1 libssl.so libcrypto.so Threads::Threads)
if(UTILS_BUILD_SHARED_LIBS)
target_link_libraries(utils libssl.so libcrypto.so)
endif()
endif()

if(WIN32)
if(UTILS_BUILD_SHARED_LIBS)
add_custom_command(
  TARGET utils POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "test/libutils.dll"
)
add_custom_command(
  TARGET utils POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "tool/libutils.dll"
)
endif()
endif()

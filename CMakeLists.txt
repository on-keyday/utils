cmake_minimum_required(VERSION 3.16)
project(utils)

if(WIN32)
include_directories("D:/OpenSSL/build-x64-debug/include")
link_directories("D:/OpenSSL/build-x64-debug/lib")
else()
# find_package(OPENSSL REQUIRED)
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
link_directories("/lib/llvm-12/lib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")
add_executable(ifacegen "src/tool/ifacegen/interface_gen.cpp"
                        "src/tool/ifacegen/read_interface.cpp"
                        "src/tool/ifacegen/generate_interface.cpp")
add_executable(binred "src/tool/binred/binred.cpp"
                      "src/tool/binred/read_fmt.cpp"
                      "src/tool/binred/generate_cpp.cpp")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")
if(WIN32)
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()
add_library(utils
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
  "src/lib/platform/windows/io_completion_port.cpp")

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

target_link_libraries(ifacegen utils)
target_link_libraries(binred utils)

if(WIN32)
target_link_libraries(netcore libssl libcrypto ws2_32)
target_link_libraries(http1 libssl libcrypto ws2_32)
target_link_libraries(io_completion_port libssl libcrypto ws2_32)
else()
target_link_libraries(netcore libssl.so libcrypto.so Threads::Threads)
target_link_libraries(http1 libssl.so libcrypto.so Threads::Threads)
endif()





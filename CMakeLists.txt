cmake_minimum_required(VERSION 3.16)
project(netiface)

if(WIN32)
link_directories("D:/OpenSSL/build-x64-debug/lib")
else()
# find_package(OPENSSL REQUIRED)
link_directories("/lib/x86_64-linux-gnu")
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

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")
add_library(libutils
  "src/lib/file/platform.cpp"
  "src/lib/file/file_view.cpp"
  "src/lib/wrap/argv.cpp"
  "src/lib/wrap/cout.cpp")

target_link_libraries(fileview libutils)
target_link_libraries(cout libutils)
target_link_libraries(channel libutils)
target_link_libraries(tokenizer libutils)
target_link_libraries(make_parser libutils)

#if(WIN32)
#target_link_libraries(netiface libssl libcrypto ws2_32)
#else()
#target_link_libraries(netiface libssl.so libcrypto.so Threads::Threads)
#endif()





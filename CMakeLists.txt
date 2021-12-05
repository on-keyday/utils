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
add_executable(core "src/test/test_core.cpp")
add_executable(utfconvert "src/test/test_utfconvert.cpp")
add_executable(fallback "src/test/test_fallback_sequence.cpp")
add_executable(utfview "src/test/test_utfview.cpp")
add_executable(utf_operator "src/test/test_utf_operator.cpp")
add_executable(endian "src/test/test_endian.cpp")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")
add_library(libutils
  "src/lib/file/platform.cpp"
  "src/lib/file/file_view.cpp")



#if(WIN32)
#target_link_libraries(netiface libssl libcrypto ws2_32)
#else()
#target_link_libraries(netiface libssl.so libcrypto.so Threads::Threads)
#endif()





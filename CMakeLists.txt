cmake_minimum_required(VERSION 3.16)
project(netiface)

if(WIN32)
link_directories("D:/OpenSSL/build-x64-debug/lib")
else()
# find_package(OPENSSL REQUIRED)
link_directories("/lib/x86_64-linux-gnu")
endif()

include_directories("src/commonlib"  "D:/OpenSSL/build-x64/include")

set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)

add_executable(test_core "src/test/test_core.cpp")
add_executable(test_utfconvert "src/test/test_utfconvert.cpp")
add_executable(test_fallback "src/test/test_fallback_sequence.cpp")

#if(WIN32)
#target_link_libraries(netiface libssl libcrypto ws2_32)
#else()
#target_link_libraries(netiface libssl.so libcrypto.so Threads::Threads)
#endif()





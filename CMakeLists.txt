#    utils - utility library
#    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
#    Released under the MIT license
#    https://opensource.org/licenses/mit-license.php
cmake_minimum_required(VERSION 3.16)
project(utils)
include_directories("src/include")

set(CMAKE_CXX_STANDARD 20)


if(WIN32)

  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # add_compile_options("-fcoroutines-ts")
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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fcoroutines-ts -fsanitize=address -fsanitize=undefined -Wunused-variable")
    set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "-nostdinc++ -nodefaultlibs -isystem /lib/llvm-12/include/c++/v1 -fuse-ld=lld")
    set(CMAKE_EXE_LINKER_FLAGS "-lc++ -lc++abi -lm -lc -lgcc_s -lgcc -lpthread -fcoroutines")
  endif()
endif()

# finding dependency
find_package(Threads REQUIRED)
find_package(LLVM CONFIG) # optional

if(LLVM_FOUND)
include_directories(${LLVM_INCLUDE_DIRS})
endif()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")

# tests(utils)
add_executable(core "src/test/core/test_core.cpp")
add_executable(utfconvert "src/test/utf/test_utfconvert.cpp")
add_executable(fallback "src/test/utf/test_fallback_sequence.cpp")
add_executable(utfview "src/test/utf/test_utfview.cpp")
add_executable(fileview "src/test/file/test_file_view.cpp")
add_executable(cout "src/test/wrap/test_cout.cpp")
add_executable(channel "src/test/thread/test_channel.cpp")
add_executable(number "src/test/number/test_number.cpp")
add_executable(to_string "src/test/number/test_to_string.cpp")
add_executable(worker "src/test/async/test_worker.cpp")
add_executable(cookie "src/test/net_util/test_cookie.cpp")
add_executable(strutil "src/test/helper/test_strutil.cpp")
add_executable(escape "src/test/escape/test_escape.cpp")
add_executable(json "src/test/json/test_json.cpp")
add_executable(cin "src/test/wrap/test_cin.cpp")
add_executable(to_json "src/test/json/test_to_json.cpp")
add_executable(jsonpath "src/test/json/test_jsonpath.cpp")
add_executable(digitcount "src/test/number/test_digitcount.cpp")
add_executable(utfcast "src/test/utf/test_utfcast.cpp")
add_executable(optparse "src/test/cmdline/test_optparse.cpp")
add_executable(optctx "src/test/cmdline/test_optctx.cpp")
add_executable(subcmd "src/test/cmdline/test_subcmd.cpp")
add_executable(coroutine "src/test/async/test_coroutine.cpp")
add_executable(run_on_single_thread "src/test/async/test_run_on_single_thread.cpp")
add_executable(make_arg "src/test/async/light/test_make_arg.cpp")
add_executable(shared_context "src/test/async/light/test_shared_context.cpp")
add_executable(taskpool2 "src/test/async/light/test_taskpool2.cpp")
add_executable(dispatch_json "src/test/json/test_dispatch_json.cpp")
add_executable(minl_def "src/test/minilang/test_minilangdef.cpp")
add_executable(ipparse "src/test/net_util/test_ipparse.cpp")
add_executable(expand_iovec "src/test/view/test_expand_vec.cpp")
add_executable(yaml_lexer "src/test/yaml/test_yaml_lexer.cpp")
add_executable(hpack "src/test/net_util/test_hpack.cpp")
add_executable(comb "src/test/minilang/test_comb.cpp")
add_executable(huffman "src/test/file/test_huffman.cpp")
add_executable(deflate "src/test/file/test_deflate.cpp")
add_executable(comb2 "src/test/comb2/test_comb2.cpp")
add_executable(reflect "src/test/reflect/test_reflect.cpp")
add_executable(qpack "src/test/net_util/test_qpack.cpp")
add_executable(span "src/test/view/test_span.cpp")
add_executable(unicode_data "src/test/unicode/test_unicode_data.cpp")

# tests(fnet)
add_executable(fnet_socket "src/test/fnet/test_fnet_socket.cpp")
add_executable(fnet_tls "src/test/fnet/test_fnet_tls.cpp")
add_executable(fnet_http "src/test/fnet/test_fnet_http.cpp")
add_executable(fnet_http2 "src/test/fnet/test_fnet_http2.cpp")
add_executable(fnetquic_initial "src/test/fnet/test_fnetquic_initial.cpp")
add_executable(fnetquic_context "src/test/fnet/test_fnetquic_context.cpp")
add_executable(fnet_error "src/test/fnet/test_fnet_error.cpp")
add_executable(fnet_stun "src/test/fnet/test_fnet_stun.cpp")
add_executable(fnetquic_frame "src/test/fnet/test_fnetquic_frame.cpp")
add_executable(fnetquic_packet "src/test/fnet/test_fnetquic_packet.cpp")
add_executable(fnetquic_stream "src/test/fnet/test_fnetquic_stream.cpp")
add_executable(fnetquic_multi_thread "src/test/fnet/test_fnetquic_multi_thread.cpp")
add_executable(fnet_dns "src/test/fnet/test_fnet_dns.cpp")



set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")
# tools
add_executable(ifacegen "src/tool/ifacegen/interface_gen.cpp"
  "src/tool/ifacegen/read_interface.cpp"
  "src/tool/ifacegen/generate_interface.cpp")
add_executable(binred "src/tool/binred/binred.cpp"
  "src/tool/binred/read_fmt.cpp"
  "src/tool/binred/generate_cpp.cpp")
add_executable(minilang "src/tool/minilang/minilang.cpp"
  "src/tool/minilang/minilang_runtime.cpp"
  "src/tool/minilang/minilang_llvm.cpp"
  "src/tool/minilang/minilang_stream.cpp"
  "src/tool/minilang/main.cpp")
add_executable(durl "src/tool/durl/main.cpp"
  "src/tool/durl/uri.cpp"
  "src/tool/durl/http.cpp"
)
add_executable(server "src/tool/server/server.cpp")
add_executable(combl "src/tool/combl/combl.cpp"
"src/tool/combl/traverse.cpp"
"src/tool/combl/ipret.cpp")
add_executable(binp "src/tool/binp/main.cpp"
"src/tool/binp/collect.cpp"
"src/tool/binp/gen.cpp")

if(LLVM_FOUND)
target_sources(combl PRIVATE "src/tool/combl/compile.cpp")
endif()


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "lib")

if(WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

option(UTILS_BUILD_SHARED_LIBS "build utils as a shared library" ON)

# libraries
add_library(utils SHARED)
add_library(fnet SHARED)
add_library(fnetserv SHARED)

# libutils
target_sources(utils
  PRIVATE
  "src/lib/file/platform.cpp"
  "src/lib/file/file_view.cpp"
  "src/lib/wrap/argv.cpp"
  "src/lib/wrap/cout.cpp"
  "src/lib/json/json_object.cpp"
  "src/lib/wrap/iocommon.cpp"
  "src/lib/wrap/cin.cpp"
  "src/lib/testutil/alloc_hook.cpp"
  "src/lib/async/light/native_context.cpp"
  "src/lib/async/light/native_stack.cpp"
  "src/lib/wrap/input.cpp"
  "src/lib/wrap/exepath.cpp"
  "src/lib/deprecated/syntax/default_syntaxc.cpp" # deprecated but dependency uses
  "src/lib/async/worker.cpp"
)


# libfnet
target_sources(fnet PRIVATE
  "src/lib/fnet/dll/lazy.cpp"
  "src/lib/fnet/socket.cpp"
  "src/lib/fnet/addrinfo.cpp"
  "src/lib/fnet/tls.cpp"
  "src/lib/fnet/tlsopt.cpp"
  "src/lib/fnet/http2.cpp"
  "src/lib/fnet/heaps.cpp"
  "src/lib/fnet/address.cpp"
  "src/lib/fnet/sockopt.cpp"
  "src/lib/fnet/error.cpp"
  "src/lib/fnet/quic/crypto/quic_tls.cpp"
  "src/lib/fnet/quic/crypto/enc_keys.cpp"
  "src/lib/fnet/quic/crypto/masks.cpp"
  "src/lib/fnet/quic/crypto/enc_packet.cpp"
  "src/lib/fnet/quic/crypto/cipher_payload.cpp"
)



if(WIN32)
  target_sources(fnet PRIVATE
    "src/lib/fnet/winsock.cpp"
  )
else()
  target_sources(fnet PRIVATE
    "src/lib/fnet/epoll.cpp"
  )
  target_compile_definitions(fnet PRIVATE _DEBUG=1)
endif()

# fnetserv
target_sources(fnetserv PRIVATE
  "src/lib/fnet/server/state.cpp"
  "src/lib/fnet/server/httpserv.cpp"
)



# test (utils)
target_link_libraries(fileview utils)
target_link_libraries(cout utils)
target_link_libraries(channel utils Threads::Threads)
target_link_libraries(worker utils)
target_link_libraries(json utils)
target_link_libraries(cin utils)
target_link_libraries(to_json utils)
target_link_libraries(jsonpath utils)
target_link_libraries(digitcount utils)
target_link_libraries(optparse utils)
target_link_libraries(optctx utils)
target_link_libraries(subcmd utils)
target_link_libraries(coroutine utils)
target_link_libraries(run_on_single_thread utils)
target_link_libraries(shared_context utils)
target_link_libraries(taskpool2 utils)
target_link_libraries(expand_iovec utils)
target_link_libraries(comb utils)
target_link_libraries(huffman utils)
target_link_libraries(deflate utils)
target_link_libraries(qpack utils)
target_link_libraries(unicode_data utils)

# test(libfnet)
target_link_libraries(fnet_socket fnet)
target_link_libraries(fnet_tls fnet)
target_link_libraries(fnet_http fnet utils)
target_link_libraries(fnet_http2 fnet)
target_link_libraries(fnetquic_initial fnet)
target_link_libraries(fnetquic_context fnet)
target_link_libraries(fnet_error fnet)
target_link_libraries(fnet_stun fnet)
target_link_libraries(fnetquic_frame utils fnet)
target_link_libraries(fnetquic_stream utils fnet)
target_link_libraries(fnetquic_multi_thread utils fnet)
target_link_libraries(fnet_dns fnet)

# test(libfnetserv)
target_link_libraries(fnetserv fnet)

# tools
target_link_libraries(ifacegen utils)
target_link_libraries(binred utils)
target_link_libraries(minilang utils)
target_link_libraries(durl utils fnet)
target_link_libraries(server utils fnetserv)
target_link_libraries(combl utils)
target_compile_options(combl PRIVATE "-fconstexpr-steps=2147483647")
target_link_libraries(binp utils)

if(LLVM_FOUND)
message(STATUS "LLVM found")
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(combl ${llvm_libs})
else()
message(STATUS "LLVM not found")
endif()



if(WIN32)
  if(UTILS_BUILD_SHARED_LIBS)
    add_custom_command(
      TARGET utils POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "test/libutils.dll"
    )
    add_custom_command(
      TARGET fnet POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libfnet.dll" "test/libfnet.dll"
    )
   
    add_custom_command(
      TARGET utils POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "tool/libutils.dll"
    )
    add_custom_command(
      TARGET fnet POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libfnet.dll" "tool/libfnet.dll"
    )
    add_custom_command(
      TARGET fnetserv POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libfnetserv.dll" "tool/libfnetserv.dll"
    )

  endif()
endif()

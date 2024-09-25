#    futils - utility library
#    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
#    Released under the MIT license
#    https://opensource.org/licenses/mit-license.php
cmake_minimum_required(VERSION 3.26)
project(futils)
include_directories("src/include")

set(CMAKE_CXX_STANDARD 20)


if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(NOT "$ENV{FUTILS_TARGET_TRIPLE}" STREQUAL "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -target $ENV{FUTILS_TARGET_TRIPLE}")
  endif()
endif()

if("$ENV{FUTILS_FREESTANDING}" STREQUAL "1")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -nostdlib++ -nostdinc -cxx-isystem ${CMAKE_SOURCE_DIR}/src/include/freestd -D__FUTILS_FREESTANDING__")
  message(STATUS "freestanding mode")
elseif(WIN32)

  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options("-ftemplate-backtrace-limit=0")
    add_compile_options("-fconstexpr-steps=2147483647")


  endif()
else()
  # find_package(OPENSSL REQUIRED)
  set(CMAKE_THREAD_LIBS_INIT "-lpthread")
  set(CMAKE_HAVE_THREADS_LIBRARY 1)
  set(CMAKE_USE_WIN32_THREADS_INIT 0)
  set(CMAKE_USE_PTHREADS_INIT 1)
  set(THREADS_PREFER_PTHREAD_FLAG ON)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
    add_compile_options("-fconstexpr-steps=2147483647")

    if(WASI_SDK_PREFIX)
      add_compile_definitions(_WASI_EMULATED_MMAN)
      set("${CMAKE_CXX_FLAGS} -lwasi-emulated-mman")
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # set(CMAKE_CXX_FLAGS "-nostdinc++ -nodefaultlibs -isystem /lib/llvm-12/include/c++/v1 -fuse-ld=lld")
    # set(CMAKE_EXE_LINKER_FLAGS "-lc++ -lc++abi -lm -lc -lgcc_s -lgcc -lpthread -fcoroutines")
  endif()
endif()


# finding dependency
find_package(Threads REQUIRED)
if(WIN32)
find_package(LLVM CONFIG) # optional
endif()
if(LLVM_FOUND)
include_directories(${LLVM_INCLUDE_DIRS})
endif()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")

# tests(futils)
add_executable(core "src/test/core/test_core.cpp")
add_executable(utfconvert "src/test/utf/test_utfconvert.cpp")
add_executable(retreat "src/test/utf/test_retreat.cpp")
add_executable(utfview "src/test/utf/test_utfview.cpp")
add_executable(fileview "src/test/file/test_file_view.cpp")
add_executable(cout "src/test/wrap/test_cout.cpp")
add_executable(channel "src/test/thread/test_channel.cpp")
add_executable(number "src/test/number/test_number.cpp")
add_executable(to_string "src/test/number/test_to_string.cpp")
# add_executable(worker "src/test/async/test_worker.cpp")
add_executable(cookie "src/test/fnet_util/test_cookie.cpp")
add_executable(strutil "src/test/helper/test_strutil.cpp")
add_executable(escape "src/test/escape/test_escape.cpp")
add_executable(json "src/test/json/test_json.cpp")
add_executable(cin "src/test/wrap/test_cin.cpp")
add_executable(to_json "src/test/json/test_to_json.cpp")
add_executable(jsonpath "src/test/json/test_jsonpath.cpp")
add_executable(digitcount "src/test/number/test_digitcount.cpp")
# add_executable(utfcast "src/test/utf/test_utfcast.cpp")
# add_executable(coroutine "src/test/async/test_coroutine.cpp")
# add_executable(run_on_single_thread "src/test/async/test_run_on_single_thread.cpp")
# add_executable(make_arg "src/test/async/light/test_make_arg.cpp")
# add_executable(shared_context "src/test/async/light/test_shared_context.cpp")
# add_executable(taskpool2 "src/test/async/light/test_taskpool2.cpp")

add_executable(optparse "src/test/cmdline/test_optparse.cpp")
add_executable(optctx "src/test/cmdline/test_optctx.cpp")
add_executable(subcmd "src/test/cmdline/test_subcmd.cpp")
add_executable(dispatch_json "src/test/json/test_dispatch_json.cpp")
# add_executable(minl_def "src/test/minilang/test_minilangdef.cpp")
add_executable(ipparse "src/test/fnet_util/test_ipparse.cpp")
add_executable(expand_iovec "src/test/view/test_expand_vec.cpp")
add_executable(yaml_lexer "src/test/yaml/test_yaml_lexer.cpp")
add_executable(hpack "src/test/fnet_util/test_hpack.cpp")
# add_executable(comb "src/test/minilang/test_comb.cpp")
add_executable(huffman "src/test/file/test_huffman.cpp")
add_executable(deflate "src/test/file/test_deflate.cpp")
add_executable(comb2 "src/test/comb2/test_comb2.cpp")
add_executable(reflect "src/test/reflect/test_reflect.cpp")
add_executable(qpack "src/test/fnet_util/test_qpack.cpp")
add_executable(span "src/test/view/test_span.cpp")
add_executable(unicode_data "src/test/unicode/test_unicode_data.cpp")
add_executable(quic_coro "src/test/coro/test_quic_coro.cpp")
add_executable(coro_nest "src/test/coro/test_coro_nest.cpp")
add_executable(base64 "src/test/fnet_util/test_base64.cpp")
add_executable(lhash "src/test/fnet_util/test_lhash.cpp")
add_executable(env_expand "src/test/env/test_env_expand.cpp")
add_executable(exepath "src/test/wrap/test_exepath.cpp")
add_executable(json_stringer "src/test/json/test_json_stringer.cpp")
add_executable(expected "src/test/helper/test_expected.cpp")
add_executable(leb128 "src/test/wasm/test_leb128.cpp")
add_executable(layout "src/test/reflect/test_layout.cpp")
add_executable(derive "src/test/math/test_derive.cpp")
add_executable(matrix "src/test/math/test_matrix.cpp")
add_executable(stack_trace "src/test/wrap/test_stack_trace.cpp")
add_executable(error_convert "src/test/error/test_error_convert.cpp")
add_executable(nan "src/test/binary/test_nan.cpp")
add_executable(derive2 "src/test/math/test_derive2.cpp")
add_executable(fft "src/test/math/test_fft.cpp")
add_executable(time_origin "src/test/timer/test_time_origin.cpp")
add_executable(io_stream "src/test/binary/test_io_stream.cpp")
add_executable(hexfilter "src/test/number/test_hexfilter.cpp")
add_executable(arbnum "src/test/binary/test_arbnum.cpp")
add_executable(jit_coroutine "src/test/jit/test_jit_coroutine.cpp")

# tests(fnet)
add_executable(fnet_socket "src/test/fnet/test_fnet_socket.cpp")
add_executable(fnet_tls "src/test/fnet/test_fnet_tls.cpp")
add_executable(fnet_http "src/test/fnet/test_fnet_http.cpp")
# add_executable(fnet_http2 "src/test/fnet/test_fnet_http2.cpp")
add_executable(fnet_http2 "src/test/fnet/test_fnet_http2_state.cpp")
# add_executable(fnetquic_initial "src/test/fnetquic/test_fnetquic_initial.cpp")
add_executable(fnetquic_context "src/test/fnetquic/test_fnetquic_context.cpp")
add_executable(fnet_error "src/test/fnet/test_fnet_error.cpp")
add_executable(fnet_stun "src/test/fnet/test_fnet_stun.cpp")
add_executable(fnetquic_frame "src/test/fnetquic/test_fnetquic_frame.cpp")
add_executable(fnetquic_packet "src/test/fnetquic/test_fnetquic_packet.cpp")
add_executable(fnetquic_stream "src/test/fnetquic/test_fnetquic_stream.cpp")
add_executable(fnetquic_multi_thread "src/test/fnetquic/test_fnetquic_multi_thread.cpp")
add_executable(fnet_dns "src/test/fnet/test_fnet_dns.cpp")
add_executable(fnetquic_0rtt "src/test/fnetquic/test_fnetquic_0rtt.cpp")
add_executable(fnetquic_internet "src/test/fnetquic/test_fnetquic_Internet.cpp")
add_executable(fnet_ip "src/test/fnet/test_fnet_ip.cpp")
add_executable(fnet_telnet "src/test/fnet/test_fnet_telnet.cpp")
add_executable(fnetquic_http3 "src/test/fnetquic/test_fnetquic_http3.cpp")

#tests(low)
add_executable(callstack "src/test/low/test_callstack.cpp")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")
# tools

add_executable(ifacegen "src/tool/ifacegen/interface_gen.cpp"
  # "src/tool/ifacegen/read_interface.cpp"
  "src/tool/ifacegen/generate_interface.cpp"
  "src/tool/ifacegen/traverse.cpp")
#[[
add_executable(binred "src/tool/binred/binred.cpp"
  "src/tool/binred/read_fmt.cpp"
  "src/tool/binred/generate_cpp.cpp")
add_executable(minilang "src/tool/minilang/minilang.cpp"
  "src/tool/minilang/minilang_runtime.cpp"
  "src/tool/minilang/minilang_llvm.cpp"
  "src/tool/minilang/minilang_stream.cpp"
  "src/tool/minilang/main.cpp")]]
add_executable(durl "src/tool/durl/main.cpp"
  "src/tool/durl/uri.cpp"
  "src/tool/durl/http.cpp"
)
add_executable(server "src/tool/server/server.cpp"
  "src/tool/server/quic_server.cpp"
)
if(LLVM_FOUND)
add_executable(combl "src/tool/combl/combl.cpp"
"src/tool/combl/traverse.cpp"
"src/tool/combl/ipret.cpp")
endif()
#[[
add_executable(binp "src/tool/binp/main.cpp"
"src/tool/binp/collect.cpp"
"src/tool/binp/gen.cpp")
]]
add_executable(unidump "src/tool/unidump/main.cpp")
add_executable(langc "src/tool/langc/main.cpp")

if(LLVM_FOUND)
target_sources(combl PRIVATE "src/tool/combl/compile.cpp")
endif()
add_executable(oslbgen "src/tool/oslbgen/main.cpp"
"src/tool/oslbgen/collect.cpp"
"src/tool/oslbgen/code.cpp")

add_executable(ping "src/tool/ping/main.cpp")

add_executable(enumgen "src/tool/enumgen/main.cpp")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "lib")

if(WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

if(NOT FUTILS_BUILD_MODE)
set(FUTILS_BUILD_MODE "shared")
endif()

# libraries
if(FUTILS_BUILD_MODE STREQUAL "shared")
add_library(futils SHARED)
add_library(fnet SHARED)
add_library(fnetserv SHARED)
add_library(coro SHARED)
add_library(low SHARED)
add_library(fraspi SHARED)
elseif(FUTILS_BUILD_MODE STREQUAL "static")
add_library(futils STATIC)
add_library(fnet STATIC)
add_library(fnetserv STATIC)
add_library(coro STATIC)
add_Library(low STATIC)
add_library(fraspi STATIC)
add_compile_definitions(FUTILS_AS_STATIC)
else()
message(FATAL_ERROR "FUTILS_BUILD_MODE must be shared or static")
endif()

# libfutils
target_sources(futils
  PRIVATE
  # "src/lib/file/platform.cpp"
  # "src/lib/file/file_view.cpp"
  "src/lib/wrap/argv.cpp"
  "src/lib/wrap/cout.cpp"
  "src/lib/json/json_object.cpp"
  # "src/lib/wrap/iocommon.cpp"
  "src/lib/wrap/cin.cpp"
  "src/lib/testutil/alloc_hook.cpp"
  "src/lib/wrap/input.cpp"
  "src/lib/wrap/exepath.cpp"
  # "src/lib/deprecated/syntax/default_syntaxc.cpp" # deprecated but dependency uses
  # "src/lib/async/worker.cpp"
  # "src/lib/async/light/native_context.cpp"
  # "src/lib/async/light/native_stack.cpp"
  "src/lib/env/env_sys.cpp"
  "src/lib/wrap/admin.cpp"
  "src/lib/wrap/trace.cpp"
  "src/lib/wrap/wasi_stub.cpp"
  "src/lib/file/file.cpp"
  "src/lib/file/console.cpp"
  "src/lib/jit/jit_memory.cpp"
)

if(WIN32)
target_sources(futils PRIVATE
  "src/lib/platform/windows/runtime_function.cpp"
)
endif()

# libfnet
target_sources(fnet PRIVATE
  "src/lib/fnet/dll/lazy.cpp"
  "src/lib/fnet/dll/load_error.cpp"
  "src/lib/fnet/socket.cpp"
  "src/lib/fnet/addrinfo.cpp"
  "src/lib/fnet/tls.cpp"
  "src/lib/fnet/tlsopt.cpp"
  #"src/lib/fnet/http2.cpp"
  "src/lib/fnet/heaps.cpp"
  "src/lib/fnet/address.cpp"
  "src/lib/fnet/sockopt.cpp"
  "src/lib/fnet/error.cpp"
  "src/lib/fnet/quic/crypto/quic_tls.cpp"
  "src/lib/fnet/quic/crypto/enc_keys.cpp"
  "src/lib/fnet/quic/crypto/masks.cpp"
  "src/lib/fnet/quic/crypto/enc_packet.cpp"
  "src/lib/fnet/quic/crypto/cipher_payload.cpp"

  "src/lib/fnet/io_event.cpp"
)



if(WIN32)
  target_sources(fnet PRIVATE
    "src/lib/fnet/winsock2.cpp"
  )
elseif(LINUX)
  target_sources(fnet PRIVATE
    "src/lib/fnet/epoll2.cpp"
  )
  target_compile_definitions(fnet PRIVATE _DEBUG=1)
endif()

# fnetserv
target_sources(fnetserv PRIVATE
  "src/lib/fnet/server/state.cpp"
  "src/lib/fnet/server/httpserv.cpp"
)

# coro
if(WIN32)
target_sources(coro PRIVATE 
  "src/lib/coro/coro_win.cpp"
)
else()
target_sources(coro PRIVATE
  "src/lib/coro/coro_linux.cpp"
)
endif()

# low
target_sources(low PRIVATE
  "src/lib/low/stack.cpp"
)

# fraspi
target_sources(fraspi PRIVATE
  "src/lib/low/raspi/gpio.cpp"
)


# test (futils)
target_link_libraries(fileview futils)
target_link_libraries(cout futils)
target_link_libraries(channel futils Threads::Threads)
#[[ target_link_libraries(worker futils)
target_link_libraries(coroutine futils)
target_link_libraries(run_on_single_thread futils)
target_link_libraries(shared_context futils)
target_link_libraries(taskpool2 futils)
]]
target_link_libraries(json futils)
target_link_libraries(cin futils)
target_link_libraries(to_json futils)
target_link_libraries(jsonpath futils)
target_link_libraries(digitcount futils)
target_link_libraries(optparse futils)
target_link_libraries(optctx futils)
target_link_libraries(subcmd futils)
target_link_libraries(env_expand futils)

target_link_libraries(expand_iovec futils)
# target_link_libraries(comb futils)
target_link_libraries(huffman futils)
target_link_libraries(deflate futils)
target_link_libraries(qpack futils)
target_link_libraries(unicode_data futils)
target_link_libraries(exepath futils)
target_link_libraries(derive futils)
target_link_libraries(stack_trace futils)
target_link_libraries(derive2 futils)
target_link_libraries(fft futils)
target_link_libraries(hpack futils)
target_link_libraries(jit_coroutine futils)

# test(libfnet)
target_link_libraries(fnet_socket fnet)
target_link_libraries(fnet_tls fnet)
target_link_libraries(fnet_http fnet futils)
# target_link_libraries(fnet_http2 fnet)
# target_link_libraries(fnetquic_initial fnet)
target_link_libraries(fnetquic_context fnet)
target_link_libraries(fnet_error fnet)
target_link_libraries(fnet_stun fnet)
target_link_libraries(fnetquic_frame futils fnet)
target_link_libraries(fnetquic_stream futils fnet)
target_link_libraries(fnetquic_multi_thread futils fnet)
target_link_libraries(fnet_dns fnet)
target_link_libraries(fnetquic_0rtt fnet futils)
target_link_libraries(fnetquic_internet fnet futils)
target_link_libraries(fnet_telnet fnet)
target_link_libraries(fnetquic_http3 fnet futils)

# test(libfnetserv)
target_link_libraries(fnetserv fnet)

# test(libcoro)
target_link_libraries(quic_coro coro fnet futils)
target_link_libraries(coro_nest coro futils)

# test(liblow)
target_link_libraries(callstack low)

# tools
target_link_libraries(ifacegen futils)
# target_link_libraries(binred futils)
# target_link_libraries(minilang futils)
target_link_libraries(durl futils fnet)
target_link_libraries(server futils fnet fnetserv)
if(LLVM_FOUND)
target_link_libraries(combl futils)
endif()
# target_link_libraries(binp futils)
target_link_libraries(unidump futils)
target_link_libraries(oslbgen futils)
target_link_libraries(langc futils)
target_link_libraries(ping futils fnet coro)
target_link_libraries(enumgen futils)


if(LLVM_FOUND)
message(STATUS "LLVM found")
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(combl ${llvm_libs})
else()
message(STATUS "LLVM not found")
endif()



set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(DIRECTORY "${CMAKE_BINARY_DIR}/tool" DESTINATION "." OPTIONAL USE_SOURCE_PERMISSIONS )
install(DIRECTORY "${CMAKE_BINARY_DIR}/lib" DESTINATION "." OPTIONAL USE_SOURCE_PERMISSIONS )
install(DIRECTORY "${CMAKE_BINARY_DIR}/test" DESTINATION "." OPTIONAL USE_SOURCE_PERMISSIONS )
install(TARGETS futils coro fnet fnetserv low fraspi DESTINATION "lib"  OPTIONAL)
install(TARGETS futils coro fnet fnetserv low fraspi DESTINATION "test" OPTIONAL)
install(TARGETS futils coro fnet fnetserv low fraspi DESTINATION "tool" OPTIONAL)

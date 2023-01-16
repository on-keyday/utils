#    utils - utility library
#    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
#    Released under the MIT license
#    https://opensource.org/licenses/mit-license.php
cmake_minimum_required(VERSION 3.16)
project(utils)
include_directories("src/include")

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
  include_directories("$ENV{OPENSSL_INCLUDE}")
  link_directories("$ENV{OPENSSL_LINK}")

  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options("-fcoroutines-ts")
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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fcoroutines-ts -fsanitize=address -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "-nostdinc++ -nodefaultlibs -isystem /lib/llvm-12/include/c++/v1 -fuse-ld=lld")
    set(CMAKE_EXE_LINKER_FLAGS "-lc++ -lc++abi -lm -lc -lgcc_s -lgcc -lpthread -fcoroutines")
  endif()
endif()

find_package(Threads REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "test")
add_executable(core "src/test/core/test_core.cpp")
add_executable(utfconvert "src/test/utf/test_utfconvert.cpp")
add_executable(fallback "src/test/utf/test_fallback_sequence.cpp")
add_executable(utfview "src/test/utf/test_utfview.cpp")
add_executable(endian "src/test/endian/test_endian.cpp")
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
add_executable(dnet_socket "src/test/dnet/test_dnet_socket.cpp")
add_executable(dnet_tls "src/test/dnet/test_dnet_tls.cpp")
add_executable(dnet_http "src/test/dnet/test_dnet_http.cpp")
add_executable(dnet_http2 "src/test/dnet/test_dnet_http2.cpp")
add_executable(minl_def "src/test/minilang/test_minilangdef.cpp")
add_executable(dnetquic_initial "src/test/dnet/test_dnetquic_initial.cpp")
add_executable(dnetquic_context "src/test/dnet/test_dnetquic_context.cpp")
add_executable(dnet_error "src/test/dnet/test_dnet_error.cpp")
add_executable(ipparse "src/test/net_util/test_ipparse.cpp")
add_executable(dnet_stun "src/test/dnet/test_dnet_stun.cpp")
add_executable(expand_iovec "src/test/view/test_expand_vec.cpp")
add_executable(dnetquic_frame "src/test/dnet/test_dnetquic_frame.cpp")
add_executable(dnetquic_packet "src/test/dnet/test_dnetquic_packet.cpp")
add_executable(dnetquic_stream "src/test/dnet/test_dnetquic_stream.cpp")

#[[
add_executable(predefined "src/test/deprecated/tokenize/test_predefined.cpp")
add_executable(tokenizer "src/test/deprecated/tokenize/test_tokenizer.cpp")
add_executable(make_parser "src/test/deprecated/syntax/test_make_parser_clang_bug.cpp")
add_executable(syntaxc "src/test/deprecated/syntax/test_syntaxc.cpp")
add_executable(netcore "src/test/deprecated/net/test_netcore.cpp")
add_executable(urlparse "src/test/deprecated/net/test_urlparse.cpp")
add_executable(base64 "src/test/deprecated/net/test_base64.cpp")
add_executable(urlencode "src/test/deprecated/net/test_urlencode.cpp")
add_executable(sha1 "src/test/deprecated/net/test_sha1.cpp")
add_executable(punycode "src/test/deprecated/net/test_punycode.cpp")
add_executable(http1 "src/test/deprecated/net/test_http1.cpp")
add_executable(dispatcher "src/test/deprecated/syntax/test_dispatcher.cpp")
add_executable(tree "src/test/deprecated/syntax/test_tree.cpp")
add_executable(parser "src/test/deprecated/parser/test_parser.cpp")
add_executable(asyncdns "src/test/deprecated/net/async/test_asyncdns.cpp")
add_executable(asynctcp "src/test/deprecated/net/async/test_asynctcp.cpp")
add_executable(http2frame "src/test/deprecated/net/test_http2frame.cpp")
add_executable(asynctcp2 "src/test/deprecated/net/async/test_asynctcp2.cpp")
add_executable(http2base "src/test/deprecated/net/async/test_http2base.cpp")
add_executable(http2protocol "src/test/deprecated/net/async/test_http2protocol.cpp")
add_executable(cnettcp "src/test/deprecated/cnet/test_cnettcp.cpp")
add_executable(cnetserver "src/test/deprecated/cnet/test_cnetserver.cpp")
add_executable(expression "src/test/deprecated/parser/test_expression.cpp")
add_executable(cnethttp2 "src/test/deprecated/cnet/test_cnethttp2.cpp")
add_executable(cnetmem "src/test/deprecated/cnet/test_cnetmem.cpp")
add_executable(base_iface "src/test/deprecated/iface/test_base_iface.cpp")
add_executable(guessurl "src/test/deprecated/net/test_guessurl.cpp")
add_executable(container_iface "src/test/deprecated/iface/test_container.cpp")
add_executable(token_stream "src/test/deprecated/parser/stream/test_token_stream.cpp")
add_executable(quic_varint "src/test/deprecated/quic/test_quic_varint.cpp")
add_executable(quic_udp "src/test/deprecated/quic/test_quic_udp.cpp")
add_executable(quic_shared_ptr "src/test/deprecated/quic/test_quic_shared_ptr.cpp")
add_executable(quic_event "src/test/deprecated/quic/test_quic_event.cpp")
add_executable(quic_frame "src/test/deprecated/quic/test_quic_frame.cpp")
add_executable(quic_crypto "src/test/deprecated/quic/test_quic_crypto.cpp")
add_executable(quic_hash_map "src/test/deprecated/quic/test_quic_hash_map.cpp")
add_executable(quic_init_conn "src/test/deprecated/quic/test_quic_init_conn.cpp")
add_executable(externaltask "src/test/async/test_externaltask.cpp")
]]

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")
add_executable(ifacegen "src/tool/ifacegen/interface_gen.cpp"
  "src/tool/ifacegen/read_interface.cpp"
  "src/tool/ifacegen/generate_interface.cpp")
add_executable(binred "src/tool/binred/binred.cpp"
  "src/tool/binred/read_fmt.cpp"
  "src/tool/binred/generate_cpp.cpp")
add_executable(pscmpl "src/tool/pscmpl/pscmpl.cpp"
  "src/tool/pscmpl/compile.cpp")
add_executable(minilang "src/tool/minilang/minilang.cpp"
  "src/tool/minilang/minilang_runtime.cpp"
  "src/tool/minilang/minilang_llvm.cpp"
  "src/tool/minilang/minilang_stream.cpp"
  "src/tool/minilang/main.cpp")
add_executable(minlc "src/tool/minlc/minlc.cpp"
  "src/tool/minlc/middle/middle.cpp"
  "src/tool/minlc/middle/resolve.cpp"
  "src/tool/minlc/middle/type.cpp"
  "src/tool/minlc/backend/llvm/module.cpp"
)
add_executable(durl "src/tool/durl/main.cpp"
  "src/tool/durl/uri.cpp"
  "src/tool/durl/http.cpp"
)
add_executable(igen2 "src/tool/igen2/main.cpp")
add_executable(vstack "src/tool/vstack/main.cpp"
"src/tool/vstack/write.cpp"
"src/tool/vstack/resolve.cpp"
"src/tool/vstack/func.cpp"
"src/tool/vstack/ssa.cpp"
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "lib")

if(WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

option(UTILS_BUILD_SHARED_LIBS "build utils as a shared library" ON)

add_library(utils SHARED)
add_library(utnet SHARED)
add_library(dnet SHARED)
add_library(dnetserv SHARED)

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
)

# deprecated use dnet instead
target_sources(utnet
  PRIVATE
  "src/lib/async/worker.cpp"

  #[[
  "src/lib/deprecated/net/core/init_net.cpp"
  "src/lib/deprecated/net/dns/dns.cpp"
  "src/lib/deprecated/net/tcp/tcp.cpp"
  "src/lib/deprecated/net/ssl/ssl_common.cpp"
  "src/lib/deprecated/net/ssl/ssl_conn.cpp"
  "src/lib/deprecated/net/http/http1.cpp"
  "src/lib/deprecated/net/async/async_dns.cpp"
  "src/lib/deprecated/net/async/common_pool.cpp"
  "src/lib/deprecated/net/async/async_tcp.cpp"
  "src/lib/deprecated/net/ssl/ssl_async.cpp"
  "src/lib/deprecated/net/http2/conn.cpp"
  "src/lib/deprecated/net/http2/stream_update.cpp"
  "src/lib/deprecated/net/http2/stream_make.cpp"
  "src/lib/deprecated/net/http2/stream_context.cpp"
  "src/lib/deprecated/net/http2/request.cpp"
  "src/lib/deprecated/net/http2/request_methods.cpp"
  "src/lib/deprecated/cnet/cnet.cpp"
  "src/lib/deprecated/cnet/tcp.cpp"
  "src/lib/deprecated/cnet/ssl.cpp"
  "src/lib/deprecated/cnet/tcp_server.cpp"
  "src/lib/deprecated/cnet/http2.cpp"
  "src/lib/deprecated/cnet/mem_buffer.cpp"
  "src/lib/deprecated/cnet/mem_iface.cpp"
  ]]
)

target_sources(dnet PRIVATE
  "src/lib/dnet/sockdll.cpp"
  "src/lib/dnet/socket.cpp"
  "src/lib/dnet/addrinfo.cpp"
  "src/lib/dnet/ssldll.cpp"
  "src/lib/dnet/tls.cpp"
  "src/lib/dnet/http2.cpp"
  "src/lib/dnet/heaps.cpp"
  "src/lib/dnet/address.cpp"
  "src/lib/dnet/sockopt.cpp"
  "src/lib/dnet/error.cpp"
  "src/lib/dnet/quic/crypto/quic_tls.cpp"
  "src/lib/dnet/quic/crypto/enc_keys.cpp"
  "src/lib/dnet/quic/crypto/masks.cpp"
  "src/lib/dnet/quic/crypto/enc_packet.cpp"
  "src/lib/dnet/quic/crypto/cipher_payload.cpp"
)

target_sources(dnetserv PRIVATE
  "src/lib/dnet/server/state.cpp"
  "src/lib/dnet/server/httpserv.cpp"
)

if(WIN32)
  target_sources(dnet PRIVATE
    "src/lib/dnet/winsock.cpp"
  )
else()
  target_sources(dnet PRIVATE
    "src/lib/dnet/epoll.cpp"
  )
  target_compile_definitions(dnet PRIVATE _DEBUG=1)
endif()

#[[ libquic
add_library(quic SHARED)

target_sources(quic
  PRIVATE
  "src/lib/deprecated/quic/core/core.cpp"
  "src/lib/deprecated/quic/io/udp_wait.cpp"
  "src/lib/deprecated/quic/io/udp.cpp"
  "src/lib/deprecated/quic/mem/stdalloc.cpp"
  "src/lib/deprecated/quic/mem/stdbpool.cpp"
  "src/lib/deprecated/quic/core/external_func.cpp"
  "src/lib/deprecated/quic/crypto/hkdf.cpp"
  "src/lib/deprecated/quic/crypto/initial.cpp"
  "src/lib/deprecated/quic/crypto/crypto_frames.cpp"
  "src/lib/deprecated/quic/core/capi.cpp"
  "src/lib/deprecated/quic/conn/conn.cpp"
  "src/lib/deprecated/quic/flow/init.cpp"
)
]]

if(WIN32)
  # target_sources(quic PRIVATE "src/lib/quic/core/dll.rc")
endif()

# endif()
target_link_libraries(fileview utils)
target_link_libraries(cout utils)
target_link_libraries(channel utils Threads::Threads)
#[[ deprecated
target_link_libraries(tokenizer utils)
target_link_libraries(make_parser utils)
target_link_libraries(syntaxc utils)
target_link_libraries(netcore utils utnet)
target_link_libraries(http1 utils utnet Threads::Threads)
target_link_libraries(dispatcher utils)
target_link_libraries(tree utils)
target_link_libraries(parser utils)
target_link_libraries(asyncdns utils utnet)
target_link_libraries(asynctcp utils utnet)
target_link_libraries(asynctcp2 utils utnet)
target_link_libraries(http2base utils utnet)
target_link_libraries(http2protocol utils utnet)
target_link_libraries(cnettcp utils utnet)
target_link_libraries(cnetserver utils utnet)
target_link_libraries(cnethttp2 utils utnet)
target_link_libraries(cnetmem utils utnet)
target_link_libraries(container_iface utils utnet)
target_link_libraries(io_completion_port utils utnet)
target_link_libraries(externaltask utils utnet)
target_compile_options(parser PRIVATE "-ftemplate-backtrace-limit=0")
target_link_libraries(expression utils)
]]


target_link_libraries(worker utils utnet)
target_link_libraries(json utils)
target_link_libraries(cin utils)
target_link_libraries(to_json utils)
target_link_libraries(jsonpath utils)
target_link_libraries(digitcount utils)
target_link_libraries(optparse utils)
target_link_libraries(optctx utils)
target_link_libraries(subcmd utils)
target_link_libraries(coroutine utils utnet)
target_link_libraries(run_on_single_thread utils utnet)
target_link_libraries(shared_context utils utnet)
target_link_libraries(taskpool2 utils)

target_link_libraries(expand_iovec utils)




target_link_libraries(ifacegen utils)
target_link_libraries(binred utils)
target_link_libraries(pscmpl utils)
target_link_libraries(minilang utils)
target_link_libraries(minlc utils)
target_link_libraries(durl utils dnet)
target_link_libraries(igen2 utils)
target_link_libraries(vstack utils)

#[[ libquic
target_link_libraries(quic_udp quic utils)
target_link_libraries(quic_shared_ptr quic)
target_link_libraries(quic_event quic)
target_link_libraries(quic_frame quic)
target_link_libraries(quic_hash_map quic)
target_link_libraries(quic_crypto quic)
target_link_libraries(quic_init_conn quic)
]]

# libdnet
target_link_libraries(dnet_socket dnet)
target_link_libraries(dnet_tls dnet)
target_link_libraries(dnet_http dnet utils)
target_link_libraries(dnet_http2 dnet)
target_link_libraries(dnetquic_initial dnet)
target_link_libraries(dnetquic_context dnet)
target_link_libraries(dnet_error dnet)
target_link_libraries(dnet_stun dnet)
target_link_libraries(dnetquic_frame utils dnet)
target_link_libraries(dnetquic_stream utils dnet)

# libdnetserv
target_link_libraries(dnetserv dnet)

if(WIN32)
  # target_link_libraries(io_completion_port ws2_32)
  # target_link_libraries(utnet libssl libcrypto ws2_32 dnet)
  # target_link_libraries(quic ws2_32)
else()
  # target_link_libraries(utils libssl.so libcrypto.so)
  # target_link_libraries(utnet dnet)
  # target_link_libraries(asynctcp2 Threads::Threads)
endif()

if(WIN32)
  if(UTILS_BUILD_SHARED_LIBS)
    add_custom_command(
      TARGET utils POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "test/libutils.dll"
    )
    add_custom_command(
      TARGET utnet POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutnet.dll" "test/libutnet.dll"
    )
    add_custom_command(
      TARGET dnet POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libdnet.dll" "test/libdnet.dll"
    )
    #[[
    add_custom_command(
      TARGET quic POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libquic.dll" "test/libquic.dll"
    )]]
    add_custom_command(
      TARGET utils POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "tool/libutils.dll"
    )
    add_custom_command(
      TARGET utnet POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutnet.dll" "tool/libutnet.dll"
    )
  endif()
endif()

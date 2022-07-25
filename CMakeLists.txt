cmake_minimum_required(VERSION 3.16)
project(utils)
include_directories("src/include")

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

set(CMAKE_CXX_STANDARD 20)

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
add_executable(predefined "src/test/tokenize/test_predefined.cpp")
add_executable(tokenizer "src/test/tokenize/test_tokenizer.cpp")
add_executable(make_parser "src/test/syntax/test_make_parser_clang_bug.cpp")
add_executable(syntaxc "src/test/syntax/test_syntaxc.cpp")
add_executable(number "src/test/number/test_number.cpp")
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
  add_executable(externaltask "src/test/async/test_externaltask.cpp")
endif()

add_executable(worker "src/test/async/test_worker.cpp")
add_executable(cookie "src/test/net/test_cookie.cpp")
add_executable(strutil "src/test/helper/test_strutil.cpp")
add_executable(dispatcher "src/test/syntax/test_dispatcher.cpp")
add_executable(tree "src/test/syntax/test_tree.cpp")
add_executable(escape "src/test/escape/test_escape.cpp")
add_executable(json "src/test/json/test_json.cpp")
add_executable(cin "src/test/wrap/test_cin.cpp")
add_executable(to_json "src/test/json/test_to_json.cpp")
add_executable(jsonpath "src/test/json/test_jsonpath.cpp")
add_executable(parser "src/test/parser/test_parser.cpp")
add_executable(digitcount "src/test/number/test_digitcount.cpp")
add_executable(utfcast "src/test/utf/test_utfcast.cpp")
add_executable(asyncdns "src/test/net/async/test_asyncdns.cpp")
add_executable(asynctcp "src/test/net/async/test_asynctcp.cpp")
add_executable(http2frame "src/test/net/test_http2frame.cpp")
add_executable(optparse "src/test/cmdline/test_optparse.cpp")
add_executable(optctx "src/test/cmdline/test_optctx.cpp")
add_executable(subcmd "src/test/cmdline/test_subcmd.cpp")
add_executable(coroutine "src/test/async/test_coroutine.cpp")
add_executable(asynctcp2 "src/test/net/async/test_asynctcp2.cpp")
add_executable(http2base "src/test/net/async/test_http2base.cpp")
add_executable(http2protocol "src/test/net/async/test_http2protocol.cpp")
add_executable(run_on_single_thread "src/test/async/test_run_on_single_thread.cpp")
add_executable(make_arg "src/test/async/light/test_make_arg.cpp")
add_executable(shared_context "src/test/async/light/test_shared_context.cpp")
add_executable(taskpool2 "src/test/async/light/test_taskpool2.cpp")
add_executable(cnettcp "src/test/cnet/test_cnettcp.cpp")
add_executable(cnetserver "src/test/cnet/test_cnetserver.cpp")
add_executable(expression "src/test/parser/test_expression.cpp")
add_executable(cnethttp2 "src/test/cnet/test_cnethttp2.cpp")
add_executable(cnetmem "src/test/cnet/test_cnetmem.cpp")
add_executable(base_iface "src/test/iface/test_base_iface.cpp")
add_executable(guessurl "src/test/net/test_guessurl.cpp")
add_executable(container_iface "src/test/iface/test_container.cpp")
add_executable(token_stream "src/test/parser/stream/test_token_stream.cpp")
add_executable(dispatch_json "src/test/json/test_dispatch_json.cpp")
add_executable(quic_varint "src/test/quic/test_quic_varint.cpp")
add_executable(quic_udp "src/test/quic/test_quic_udp.cpp")
add_executable(quic_shared_ptr "src/test/quic/test_quic_shared_ptr.cpp")
add_executable(quic_event "src/test/quic/test_quic_event.cpp")
add_executable(quic_frame "src/test/quic/test_quic_frame.cpp")
add_executable(quic_crypto "src/test/quic/test_quic_crypto.cpp")
add_executable(quic_hash_map "src/test/quic/test_quic_hash_map.cpp")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "tool")
add_executable(ifacegen "src/tool/ifacegen/interface_gen.cpp"
  "src/tool/ifacegen/read_interface.cpp"
  "src/tool/ifacegen/generate_interface.cpp")
add_executable(binred "src/tool/binred/binred.cpp"
  "src/tool/binred/read_fmt.cpp"
  "src/tool/binred/generate_cpp.cpp")
add_executable(netutil "src/tool/netutil/netmain.cpp"
  "src/tool/netutil/httpreq.cpp"
  "src/tool/netutil/http_do.cpp"
  "src/tool/netutil/debug_log.cpp"
  "src/tool/netutil/tagcmd.cpp")
add_executable(pscmpl "src/tool/pscmpl/pscmpl.cpp"
  "src/tool/pscmpl/compile.cpp")
add_executable(minilang "src/tool/minilang/minilang.cpp"
  "src/tool/minilang/minilang_runtime.cpp"
  "src/tool/minilang/minilang_llvm.cpp"
  "src/tool/minilang/minilang_stream.cpp"
  "src/tool/minilang/main.cpp")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "gentest")

add_executable(pscmplgen "src/gentest/pscmpl.cpp")

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
  add_compile_options(-D UTILS_AS_DLL)
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
  "src/lib/platform/linux/epoll.cpp"
  "src/lib/json/json_object.cpp"
  "src/lib/wrap/iocommon.cpp"
  "src/lib/wrap/cin.cpp"
  "src/lib/net/async/async_dns.cpp"
  "src/lib/net/async/common_pool.cpp"
  "src/lib/net/async/async_tcp.cpp"
  "src/lib/net/ssl/ssl_async.cpp"
  "src/lib/net/http2/conn.cpp"
  "src/lib/net/http2/stream_update.cpp"
  "src/lib/net/http2/stream_make.cpp"
  "src/lib/net/http2/stream_context.cpp"
  "src/lib/net/http2/request.cpp"
  "src/lib/net/http2/request_methods.cpp"
  "src/lib/testutil/alloc_hook.cpp"
  "src/lib/async/light/native_context.cpp"
  "src/lib/async/light/native_stack.cpp"
  "src/lib/cnet/cnet.cpp"
  "src/lib/cnet/tcp.cpp"
  "src/lib/cnet/ssl.cpp"
  "src/lib/cnet/tcp_server.cpp"
  "src/lib/cnet/http2.cpp"
  "src/lib/cnet/mem_buffer.cpp"
  "src/lib/cnet/mem_iface.cpp"
)

# if(WIN32)
target_sources(utils
  PRIVATE
  "src/lib/async/worker.cpp"
)

# libquic
add_library(quic SHARED)

target_sources(quic
  PRIVATE
  "src/lib/quic/core/core.cpp"
  "src/lib/quic/io/udp_wait.cpp"
  "src/lib/quic/io/udp.cpp"
  "src/lib/quic/mem/stdalloc.cpp"
  "src/lib/quic/mem/stdbpool.cpp"
  "src/lib/quic/core/external_func.cpp"
  "src/lib/quic/crypto/hkdf.cpp"
  "src/lib/quic/crypto/initial.cpp"
  "src/lib/quic/crypto/crypto_frames.cpp"
  "src/lib/quic/core/capi.cpp"
  "src/lib/quic/conn/conn.cpp"
)

if(WIN32)
  # target_sources(quic PRIVATE "src/lib/quic/core/dll.rc")
endif()

# endif()
target_link_libraries(fileview utils)
target_link_libraries(cout utils)
target_link_libraries(channel utils Threads::Threads)
target_link_libraries(tokenizer utils)
target_link_libraries(make_parser utils)
target_link_libraries(syntaxc utils)
target_link_libraries(netcore utils)
target_link_libraries(http1 utils Threads::Threads)

if(WIN32)
  target_link_libraries(io_completion_port utils)
  target_link_libraries(externaltask utils)
endif()

target_link_libraries(worker utils)
target_link_libraries(dispatcher utils)
target_link_libraries(tree utils)
target_link_libraries(json utils)
target_link_libraries(cin utils)
target_link_libraries(to_json utils)
target_link_libraries(jsonpath utils)
target_link_libraries(parser utils)
target_link_libraries(digitcount utils)
target_link_libraries(asyncdns utils)
target_link_libraries(asynctcp utils)
target_link_libraries(optparse utils)
target_link_libraries(optctx utils)
target_link_libraries(subcmd utils)
target_link_libraries(coroutine utils)
target_link_libraries(asynctcp2 utils)
target_link_libraries(http2base utils)
target_link_libraries(http2protocol utils)
target_link_libraries(run_on_single_thread utils)
target_link_libraries(shared_context utils)
target_link_libraries(taskpool2 utils)
target_link_libraries(cnettcp utils)
target_link_libraries(cnetserver utils)
target_link_libraries(expression utils)
target_link_libraries(cnethttp2 utils)
target_link_libraries(cnetmem utils)
target_link_libraries(container_iface utils)

target_compile_options(parser PRIVATE "-ftemplate-backtrace-limit=0")

target_link_libraries(ifacegen utils)
target_link_libraries(binred utils)
target_link_libraries(netutil utils)
target_link_libraries(pscmpl utils)
target_link_libraries(minilang utils)

# libquic
target_link_libraries(quic_udp quic utils)
target_link_libraries(quic_shared_ptr quic)
target_link_libraries(quic_event quic)
target_link_libraries(quic_frame quic)
target_link_libraries(quic_hash_map quic)
target_link_libraries(quic_crypto quic)

if(WIN32)
  target_link_libraries(netcore libssl libcrypto ws2_32)
  target_link_libraries(http1 libssl libcrypto ws2_32)
  target_link_libraries(io_completion_port libssl libcrypto ws2_32)

  if(UTILS_BUILD_SHARED_LIBS)
    target_link_libraries(utils libssl libcrypto ws2_32)
  endif()

  target_link_libraries(quic ws2_32)
else()
  target_link_libraries(netcore libssl.so libcrypto.so Threads::Threads)
  target_link_libraries(http1 libssl.so libcrypto.so Threads::Threads)

  if(UTILS_BUILD_SHARED_LIBS)
    target_link_libraries(utils libssl.so libcrypto.so)
  endif()

  target_link_libraries(asynctcp2 Threads::Threads)
endif()

# gentest code
target_link_libraries(pscmplgen utils)

if(WIN32)
  if(UTILS_BUILD_SHARED_LIBS)
    add_custom_command(
      TARGET utils POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "test/libutils.dll"
    )
    add_custom_command(
      TARGET quic POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libquic.dll" "test/libquic.dll"
    )
    add_custom_command(
      TARGET utils POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "tool/libutils.dll"
    )
    add_custom_command(
      TARGET utils POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "lib/libutils.dll" "gentest/libutils.dll"
    )
  endif()
endif()

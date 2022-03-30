/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp cnet interface
#pragma once
#include "../platform/windows/dllexport_header.h"
#include "cnet.h"

namespace utils {
    namespace cnet {
        namespace tcp {

            // create tcp client
            DLL CNet* STDCALL create_client();

            // create tcp server
            DLL CNet* STDCALL create_server();

            // report io completion
            DLL bool STDCALL io_completion(void* ol, size_t recvsize);

            enum TCPConfig {
                // ip version
                ipver = user_defined_start + 1,
                // retry after ::connect() failed
                retry_after_connect_call,
                // use multiple io system like below
                // windows: io completion port
                // linux: epoll
                use_multiple_io_system,
                // polling recv data
                recv_polling,
                // host name
                host_name,
                // port number
                port_number,
                // raw socket (only get)
                raw_socket,
                // current status
                current_status,

                // wait for accepting (server)
                wait_for_accept,

                // resuse address flag
                reuse_address,
            };

            enum class TCPStatus {
                unknown,
                idle,

                start_resolving_name,
                wait_resolving_name,
                resolve_name_done,

                start_connectig,
                wait_connect,
                connected,

                start_recving,
                wait_recv,
                wait_async_recv,
                recieved,

                start_sending,
                wait_send,
                sent,

                wait_accept,
            };

            inline int ip_version(CNet* ctx) {
                return query_number(ctx, ipver);
            }

            inline bool set_hostport(CNet* ctx, const char* host, const char* port) {
                return set_ptr(ctx, host_name, (void*)host) && set_ptr(ctx, port_number, (void*)port);
            }

            inline bool set_port(CNet* ctx, const char* port) {
                return set_ptr(ctx, port_number, (void*)port);
            }

            inline bool retry_after_connect(CNet* ctx, bool flag) {
                return set_number(ctx, retry_after_connect_call, flag);
            }

            inline bool use_multiple_io(CNet* ctx, bool flag) {
                return set_number(ctx, use_multiple_io_system, flag);
            }

            inline bool polling_recv(CNet* ctx, bool flag) {
                return set_number(ctx, recv_polling, flag);
            }

            inline std::uintptr_t get_raw_socket(CNet* ctx) {
                return query_number(ctx, raw_socket);
            }

            inline TCPStatus get_current_state(CNet* ctx) {
                return (TCPStatus)query_number(ctx, current_status);
            }

            inline bool is_waiting(CNet* ctx) {
                auto s = get_current_state(ctx);
                return s == TCPStatus::wait_resolving_name ||
                       s == TCPStatus::wait_connect ||
                       s == TCPStatus::wait_recv ||
                       s == TCPStatus::wait_async_recv ||
                       s == TCPStatus::wait_accept;
            }

            inline CNet* accept(CNet* ctx) {
                return (CNet*)query_ptr(ctx, wait_for_accept);
            }

            inline bool set_resuse_address(CNet* ctx, bool flag) {
                return set_number(ctx, reuse_address, flag);
            }

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl - cnet ssl context
#pragma once
#include "cnet.h"

namespace utils {
    namespace cnet {
        namespace ssl {
            DLL CNet* STDCALL create_client();

            enum TLSConfig {
                // alpn string
                alpn,
                // host name for verification
                host_verify,
                // certificate file path
                cert_file,

                // raw ssl context (get only)
                raw_ssl,

                // cuurent state (get only)
                current_state,

                // alpn selected (get only)
                alpn_selected,

                // invoke ssl error callback
                invoke_ssl_error_callback,

                // set reading timeout
                reading_timeout,

                // multi thread config
                multi_thread,

                // allow anonymous
                looser_check,
            };

            enum class TLSStatus {
                unknown,
                idle,

                start_write_to_low,
                writing_to_low,
                write_to_low_done,

                start_read_from_low,
                reading_from_low,
                read_from_low_done,

                start_connect,
                connected,

                start_read,
                read_done,

                start_write,
                write_done,
            };

            inline TLSStatus get_current_state(CNet* ctx) {
                return (TLSStatus)query_number(ctx, current_state);
            }

            inline bool set_certificate_file(CNet* ctx, const char* file) {
                return set_ptr(ctx, cert_file, (void*)file);
            }

            inline bool set_alpn(CNet* ctx, const char* alpnstr) {
                return set_ptr(ctx, alpn, (void*)alpnstr);
            }

            inline bool set_host(CNet* ctx, const char* host) {
                return set_ptr(ctx, host_verify, (void*)host);
            }

            inline const char* get_alpn_selected(CNet* ctx) {
                return (const char*)query_ptr(ctx, alpn_selected);
            }

            inline bool is_waiting(CNet* ctx) {
                auto s = get_current_state(ctx);
                return s == TLSStatus::writing_to_low ||
                       s == TLSStatus::reading_from_low;
            }

            struct ErrorCallback {
                int (*cb)(const char* msg, size_t len, void* user);
                void* user;
            };

            template <class Fn>
            bool error_callback(CNet* ctx, Fn&& fn) {
                struct Local {
                    decltype(std::addressof(fn)) ptr;
                } loc{std::addressof(fn)};
                ErrorCallback cbobj;
                cbobj.cb = [](const char* msg, size_t len, void* user) {
                    (*decltype (&loc)(user)->ptr)(msg, len);
                    return 1;
                };
                cbobj.user = &loc;
                return set_ptr(ctx, invoke_ssl_error_callback, &cbobj);
            }

            inline bool set_timeout(CNet* ctx, time_t time) {
                return set_number(ctx, reading_timeout, time);
            }

            inline bool set_loose_check(CNet* ctx, bool loose) {
                return set_number(ctx, looser_check, loose);
            }

            inline bool set_multi_thread(CNet* ctx, bool multi) {
                return set_number(ctx, multi_thread, multi);
            }

        }  // namespace ssl
    }      // namespace cnet
}  // namespace utils

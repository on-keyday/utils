/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// io_completetion_port
#pragma once

#include "dllexport_header.h"
#include "iocompletion.h"

namespace utils {
    namespace platform {
        namespace windows {
            enum class CompletionType {
                tcp_read,
            };

            template <class Fn, class Ret = void>
            struct CompletionCallback {
                using remref = std::remove_reference_t<Fn>;
                using type = std::conditional_t<std::is_same_v<Ret, void>, remref&, remref>;
                type fn;
                CompletionCallback(type& f)
                    : fn(f) {}
                Ret call(void* ol, size_t sz) {
                    return fn(ol, sz);
                }
            };

            struct IOCPContext;
            struct DLL IOCPObject {
                friend DLL IOCPObject* STDCALL get_iocp();

                bool register_handle(void* handle);

                template <class Fn>
                void wait_completion(Fn&& cb, size_t maxcount, int wait) {
                    auto fn = CompletionCallback<Fn>{cb};
                    wait_completion_impl(fn, maxcount, wait);
                }
                template <class Fn>
                void register_callback(Fn&& cb) {
                    auto fn = CompletionCallback<Fn, bool>{cb};
                    register_callback_impl(fn);
                }
#ifdef _WIN32
                void wait_callbacks(size_t maxcount, int wait);
#else
                void wait_callbacks(size_t, int) {}
#endif
               private:
                IOCPObject();
                ~IOCPObject();
                void wait_completion_impl(CCBInvoke cb, size_t maxcount, int wait);
                void register_callback_impl(CCBRegistered cb);
                IOCPContext* ctx;
            };
#ifdef _WIN32
            DLL IOCPObject* STDCALL get_iocp();
#else
            inline IOCPObject* get_iocp() {
                return nullptr;
            }
#endif
        }  // namespace windows

    }  // namespace platform
}  // namespace utils

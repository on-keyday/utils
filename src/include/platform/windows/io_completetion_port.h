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
            template <class Fn>
            struct CompletionCallback {
                Fn& fn;
                CompletionCallback(Fn& f)
                    : fn(f) {}
                void call(void* ol, size_t sz) {
                    fn(ol, sz);
                }
            };

            struct IOCPContext;
            struct DLL IOCPObject {
                friend DLL IOCPObject* STDCALL get_iocp();

                bool register_handle(void* handle);

                template <class Fn>
                void wait_completion(Fn&& cb, size_t maxcount, int wait) {
                    auto fn = CompletionCallback{cb};
                    wait_completion_impl(fn, maxcount, wait);
                }

               private:
                IOCPObject();
                ~IOCPObject();
                void wait_completion_impl(CCBInvoke cb, size_t maxcount, int wait);
                IOCPContext* ctx;
            };
            DLL IOCPObject* STDCALL get_iocp();

        }  // namespace windows

    }  // namespace platform
}  // namespace utils

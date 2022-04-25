/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// http2 - http2 interface
#pragma once
#include "cnet.h"
#include "../net/http2/frame.h"

namespace utils {
    namespace net {
        namespace http2 {
            struct Stream;
        }
    }  // namespace net
}  // namespace utils

namespace utils {
    namespace cnet {
        namespace http2 {

            DLL CNet* STDCALL create_client();

            struct Frames;

            wrap::shared_ptr<net::http2::Frame>* begin(Frames*);

            wrap::shared_ptr<net::http2::Frame>* end(Frames*);

            net::http2::Stream* get_stream(Frames*);

            enum Http2Setting {
                // invoke poll method
                poll = user_defined_start + 1,
                // register callback
                set_callback,
            };

            struct Callback {
                bool (*callback)(void*, Frames*);
                void* this_ = nullptr;
                void (*deleter)(void*);
            };

            template <class Fn>
            bool set_frame_callback(CNet* ctx, Fn fn) {
                Callback callback;
                struct Pack {
                    Fn fn;
                };
                if constexpr (sizeof(Pack) <= sizeof(void*)) {
                    Pack* ptr = reinterpret_cast<Pack*>(&callback.this_);
                    *ptr = Pack{std::move(fn)};
                    callback.callback = [](void* p, Frames* fr) {
                        Pack* ptr = reinterpret_cast<Pack*>(&p);
                        return ptr->fn(fr);
                    };
                    callback.deleter = nullptr;
                }
                else {
                    callback.this_ = new Pack{fn};
                    callback.this_ = [](void* p, Frames* fr) {
                        Pack* ptr = static_cast<Pack*>(p);
                        return ptr->fn(fr);
                    };
                    callback.deleter = [](void* p) {
                        Pack* ptr = static_cast<Pack*>(p);
                        delete ptr;
                    };
                }
                return cnet::set_ptr(ctx, set_callback, &callback);
            }

            bool poll_frame(CNet* ctx) {
                return cnet::set_ptr(ctx, poll, nullptr);
            }
        }  // namespace http2
    }      // namespace cnet
}  // namespace utils

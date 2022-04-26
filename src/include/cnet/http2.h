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
#include "../helper/pushbacker.h"

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

            DLL wrap::shared_ptr<net::http2::Frame>* STDCALL begin(Frames*);

            DLL wrap::shared_ptr<net::http2::Frame>* STDCALL end(Frames*);

            DLL net::http2::Stream* STDCALL get_stream(Frames*);

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

            inline bool poll_frame(CNet* ctx) {
                return cnet::set_ptr(ctx, poll, nullptr);
            }

            enum class DefaultProc {
                none,
                send_window_update = 0x1,
                ping = 0x2,
                settings = 0x4,
                all = send_window_update | ping | settings,
            };

            DEFINE_ENUM_FLAGOP(DefaultProc)

            DLL bool STDCALL default_proc(Frames* fr, wrap::shared_ptr<net::http2::Frame>& frame, DefaultProc filter);
            DLL bool STDCALL default_procs(Frames* fr, DefaultProc filter);

            struct Fetcher {
               private:
                template <class T>
                static void fetch_fn(void* ptr, helper::IPushBacker first, helper::IPushBacker second) {
                    auto iface = static_cast<T*>(ptr);
                    helper::append(first, iface->first);
                    helper::append(first, iface->second);
                }

                template <class T>
                static void increment_fn(void* ptr) {
                    auto iface = static_cast<T*>(ptr);
                    ++*iface;
                }

                template <class End, class T>
                static bool on_end_fn(void* mptr, void* ptr) {
                    auto iface = static_cast<T*>(ptr);
                    auto ends = static_cast<End*>(mptr);
                    return *iface == *ends;
                }

                void (*fetcher)(void* ptr, helper::IPushBacker first, helper::IPushBacker second);
                void (*incr)(void* ptr);
                bool (*ender)(void* ends, void* ptr);
                void* it;
                void* ends;

               public:
                Fetcher(const Fetcher&) = default;
                template <class It, class End>
                Fetcher(It& it, End& end)
                    : it(std::addressof(it)), ends(std::addressof(end)), fetcher(fetch_fn<It>), incr(increment_fn<It>), ender(on_end_fn<End, It>) {}

                void fetch(helper::IPushBacker first, helper::IPushBacker second) {
                    fetcher(it, first, second);
                    incr(it);
                }

                void prefetch(helper::IPushBacker first, helper::IPushBacker second) const {
                    fetcher(it, first, second);
                }

                bool on_end() const {
                    return ender(ends, it);
                }
            };

            DLL bool STDCALL write_header(Frames* v, Fetcher fetch, std::int32_t& id);

            template <class Header>
            bool header(Frames* fr, const Header& h, std::int32_t& id) {
                auto begin = h.begin();
                auto end = h.end();
                return write_header(fr, Fetcher{begin, end}, id);
            }
        }  // namespace http2
    }      // namespace cnet
}  // namespace utils

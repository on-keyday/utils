/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// http2 - http2 interface
#pragma once
#include "cnet.h"
#include <deprecated/net/http2/frame.h>
#include <helper/pushbacker.h>

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

            enum Http2Config {
                // invoke poll method
                config_frame_poll = user_defined_start + 1,
                // register read frame callback
                config_set_read_callback,
                // register write frame callback
                config_set_write_callback,
            };

            struct ReadCallback {
                bool (*callback)(void*, Frames*);
                void* this_ = nullptr;
                void (*deleter)(void*);
            };

            struct WriteCallback {
                void (*callback)(void*, const net::http2::Frame*);
                void* this_ = nullptr;
                void (*deleter)(void*);
            };

            template <class Callback, class Fn, class... Args>
            Callback make_callback(Fn fn) {
                Callback callback;
                struct Pack {
                    Fn fn;
                };
                if constexpr (sizeof(Pack) <= sizeof(void*)) {
                    Pack* ptr = reinterpret_cast<Pack*>(&callback.this_);
                    new (ptr) Pack{std::move(fn)};
                    callback.callback = [](void* p, Args... fr) {
                        Pack* ptr = reinterpret_cast<Pack*>(&p);
                        return ptr->fn(std::forward<Args>(fr)...);
                    };
                    callback.deleter = nullptr;
                }
                else {
                    callback.this_ = new Pack{std::move(fn)};
                    callback.callback = [](void* p, Args... fr) {
                        Pack* ptr = static_cast<Pack*>(p);
                        return ptr->fn(std::forward<Args>(fr)...);
                    };
                    callback.deleter = [](void* p) {
                        Pack* ptr = static_cast<Pack*>(p);
                        delete ptr;
                    };
                }
                return callback;
            }

            template <class Fn>
            bool set_frame_read_callback(CNet* ctx, Fn fn) {
                auto callback = make_callback<ReadCallback, Fn, Frames*>(std::move(fn));
                return cnet::set_ptr(ctx, config_set_read_callback, &callback);
            }

            template <class Fn>
            bool set_frame_write_callback(CNet* ctx, Fn fn) {
                auto callback = make_callback<WriteCallback, Fn, const net::http2::Frame*>(std::move(fn));
                return cnet::set_ptr(ctx, config_set_write_callback, &callback);
            }

            inline bool poll_frame(CNet* ctx) {
                return cnet::set_ptr(ctx, config_frame_poll, nullptr);
            }

            enum class DefaultProc {
                none,
                send_window_update = 0x1,
                ping = 0x2,
                settings = 0x4,
                all = send_window_update | ping | settings,
            };

            DEFINE_ENUM_FLAGOP(DefaultProc)

            constexpr auto preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

            DLL bool STDCALL default_proc(Frames* fr, wrap::shared_ptr<net::http2::Frame>& frame, DefaultProc filter);
            DLL bool STDCALL default_procs(Frames* fr, DefaultProc filter);

            inline bool is_settings_ack(const wrap::shared_ptr<net::http2::Frame>& frame) {
                return frame &&
                       frame->type == net::http2::FrameType::settings &&
                       frame->flag & net::http2::Flag::ack;
            }

            inline bool is_close_stream_signal(const wrap::shared_ptr<net::http2::Frame>& frame, bool include_goaway = true) {
                if (!frame) {
                    return false;
                }
                if (frame->type == net::http2::FrameType::rst_stream ||
                    (include_goaway && frame->type == net::http2::FrameType::goaway)) {
                    return true;
                }
                if (frame->type == net::http2::FrameType::data ||
                    frame->type == net::http2::FrameType::continuous) {
                    return any(frame->flag & net::http2::Flag::end_stream);
                }
                return false;
            }

            template <class Header, class Method = const char*, class Scheme = const char*>
            inline void set_request(Header& h, auto authority, auto path, Method method = "GET", Scheme scheme = "https") {
                h.emplace(":method", method);
                h.emplace(":path", path);
                h.emplace(":authority", authority);
                h.emplace(":scheme", scheme);
            }

            struct Fetcher {
               private:
                template <class T>
                static void fetch_fn(void* ptr, helper::IPushBacker first, helper::IPushBacker second) {
                    auto iface = static_cast<T*>(ptr);
                    helper::append(first, get<0>(**iface));
                    helper::append(second, get<1>(**iface));
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

            struct RFetcher {
               private:
                void (*emplacer)(void* p, const char* key, const char* value);
                void* ptr;
                template <class T>
                static void emplace_fn(void* p, const char* key, const char* value) {
                    auto f = static_cast<T*>(p);
                    f->emplace(key, value);
                }

               public:
                RFetcher(const RFetcher&) = default;

                template <class T>
                RFetcher(T& t)
                    : ptr(std::addressof(t)), emplacer(emplace_fn<T>) {}

                void emplace(const char* key, const char* value) {
                    emplacer(ptr, key, value);
                }
            };

            DLL bool STDCALL read_header(Frames* fr, RFetcher fetch, std::int32_t id);
            DLL bool STDCALL read_data(Frames* fr, helper::IPushBacker pb, std::int32_t id);
            DLL bool STDCALL write_header(Frames* fr, Fetcher fetch, std::int32_t& id, bool no_data, net::http2::Priority* prio);

            DLL bool STDCALL write_window_update(Frames* fr, std::uint32_t increment, std::int32_t id);
            DLL void STDCALL write_goaway(Frames* fr, std::uint32_t code, bool notify_graceful = false);

            inline void write_goaway(Frames* fr, net::http2::H2Error code, bool notify_geraceful = false) {
                return write_goaway(fr, std::uint32_t(code), notify_geraceful);
            }

            DLL bool STDCALL flush_write_buffer(Frames* fr);

            template <class Header>
            bool write_header(Frames* fr, const Header& h, std::int32_t& id, bool no_data = true, net::http2::Priority* prio = nullptr) {
                auto begin = h.begin();
                auto end = h.end();
                return write_header(fr, Fetcher{begin, end}, id, no_data, prio);
            }
        }  // namespace http2
    }      // namespace cnet
}  // namespace utils

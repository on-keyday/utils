/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// http2 - hyper text transfer protcol version 2
#pragma once
#include "dll/dllh.h"
#include <utility>
#include <memory>
#include "h2err.h"
#include "h2frame.h"
#include "h2state.h"

namespace utils {
    namespace dnet {

        namespace concepts {
            template <class T>
            concept has_view = requires(T t) {
                {t.size()};
                {t[1]};
            };

            template <class T>
            concept has_iter = requires(T t) {
                {t.begin()};
                {t.end()};
            };
        }  // namespace concepts

        namespace internal {
            using increment_access = void (*)(void* iter);
            using end_iter = bool (*)(void* iter, const void* cmp);
            using load_data = void (*)(void* iter, bool key, void* c, void (*cb)(void* c, char));
            struct data_set {
                void* iter;
                const void* cmp;
                increment_access incr;
                end_iter is_end;
                load_data load;
            };
        }  // namespace internal

        struct dnet_class_export HTTP2Stream {
           private:
            friend struct HTTP2;
            void* opt = nullptr;
            int id = -1;
            constexpr HTTP2Stream(void* o, int i)
                : opt(o), id(i) {}

            bool write_hpack_header(int id, void* opt, internal::data_set data,
                                    std::uint8_t* padding, h2frame::Priority* prio, bool end_stream,
                                    ErrCode& errc);

           public:
            constexpr HTTP2Stream() {}
            constexpr explicit operator bool() const {
                return opt != nullptr;
            }

            // write_header writes HEADER frames
            // this function would not validate values
            // Header type requires
            // - auto it = h.begin()
            // - const auto end_ =  h.end()
            // - auto&& value = get<0>(*it)
            // - get<1>(*it)
            // - ++*it
            // and one of below
            // - value[1]
            // - value.size()
            // or
            // - value.begin()
            // - value.end()
            template <class Header>
            bool write_header(Header&& h,
                              bool end_stream = false,
                              h2frame::Priority* prio = nullptr,
                              std::uint8_t* padding = nullptr, ErrCode* errc = nullptr) {
                auto it = h.begin();
                const auto end = h.end();
                using it_t = decltype(it);
                using end_t = decltype(end);
                internal::data_set set;
                set.iter = std::addressof(it);
                set.cmp = std::addressof(end);
                set.incr = [](void* i) {
                    auto it = static_cast<it_t*>(i);
                    ++*it;
                };
                set.is_end = [](void* i, const void* c) {
                    auto it = static_cast<it_t*>(i);
                    auto end_ = static_cast<end_t*>(c);
                    return *it == *end_;
                };
                set.load = [](void* iter, bool key, void* c, void (*cb)(void* c, char)) {
                    auto it = static_cast<it_t*>(iter);
                    auto&& obj = **it;
                    auto do_load = [&](auto&& base) {
                        using base_t = decltype(base);
                        if constexpr (concepts::has_iter<base_t>) {
                            for (auto&& t : base) {
                                cb(c, t);
                            }
                        }
                        else if constexpr (concepts::has_view<base_t>) {
                            auto size_ = base.size();
                            for (decltype(size_) i = 0; i < size_; i++) {
                                cb(c, base[i]);
                            }
                        }
                        else {
                            static_assert(concepts::has_iter<base_t> || concepts::has_view<base_t>,
                                          "no iterator or sized view for map string");
                        }
                    };
                    if (key) {
                        do_load(get<0>(obj));
                    }
                    else {
                        do_load(get<1>(obj));
                    }
                };
                ErrCode errcs;
                auto res = write_hpack_header(id, opt, set, padding, prio, end_stream, errcs);
                if (!res) {
                    if (errc) {
                        *errc = errcs;
                    }
                }
                return res;
            }
        };

        using HTTP2StreamCallback = void (*)(void*, h2frame::Frame&, h2stream::StreamNumState);
        using HTTP2ConnectionCallback = void (*)(void*, h2frame::Frame&, const h2stream::ConnState&);

        struct dnet_class_export HTTP2 {
           private:
            void* opt = nullptr;
            int err = 0;
            constexpr HTTP2(void* p)
                : opt(p) {}
            friend dnet_dll_export(HTTP2) create_http2(bool server, h2set::PredefinedSettings settings);

           public:
            constexpr HTTP2()
                : HTTP2(nullptr) {}
            ~HTTP2();
            constexpr HTTP2(HTTP2&& h2)
                : opt(std::exchange(h2.opt, nullptr)) {}
            HTTP2& operator=(HTTP2&& h2) {
                if (this == &h2) {
                    return *this;
                }
                this->~HTTP2();
                opt = std::exchange(h2.opt, nullptr);
                return *this;
            }
            bool provide_http2_data(const void* data, size_t size);
            bool receive_http2_data(void* data, size_t size, size_t* red);

            // progress read frames and update state
            bool progress();

            // set_stream_callback sets stream callback
            // this function is called per frame at progress function
            // f.id is always non 0
            void set_stream_callback(HTTP2StreamCallback cb, void* user);

            // set_connection_callback sets connection callback
            // this function is called per frame at progress function
            // f.id is always 0
            void set_connection_callback(HTTP2ConnectionCallback cb, void* user);

            // get_conn_err returns connection error record
            ErrCode get_conn_err();
            // closed tells connection closed
            bool closed() const;

            constexpr explicit operator bool() const {
                return opt != nullptr;
            }

            // create_stream creates new stream
            // if new_id!=0 this function try to create
            // stream with new_id
            HTTP2Stream create_stream(int new_id = 0);

            // get_stream gets exists streams
            HTTP2Stream get_stream(int id);
            // fast_open makes connection state open
            // if fast open called before other method is called
            // connection preface would not be generated
            void fast_open();

            // write_frame writes frame directry
            // this function is unsafe
            bool write_frame(h2frame::Frame& frame, ErrCode& errs);

            // flush_preface flushs connection prefaces
            bool flush_preface();
        };

        dnet_dll_export(HTTP2) create_http2(bool server, h2set::PredefinedSettings settings = {});
    }  // namespace dnet
}  // namespace utils

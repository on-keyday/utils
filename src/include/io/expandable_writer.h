/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "writer.h"

namespace utils {
    namespace io {
        template <class T>
        concept has_resize = requires(T t) {
            { t.resize(size_t{}) };
        };

        template <class T, class C, class U>
        struct basic_expand_writer {
           private:
            T buf;
            basic_writer<C, U> w;

           public:
            constexpr basic_expand_writer()
                : w(buf) {}

            constexpr basic_expand_writer(auto&& value)
                : buf(std::forward<decltype(value)>(value)), w(buf) {}

            constexpr basic_writer<C, U> writer() const {
                return w;
            }

            // resize if buf.resize is enabled
            constexpr void resize(size_t size) {
                if constexpr (has_resize<T>) {
                    buf.resize(size);
                    w.reset(buf, w.offset());
                }
            }

            constexpr void expand(size_t delta) {
                resize(buf.size() + delta);
            }

            constexpr void maybe_expand(size_t need) {
                if (w.remain().size() < need) {
                    expand(need - w.remain().size());
                }
            }

            constexpr bool write(C data, size_t n) {
                if (w.remain().size() < n) {
                    expand(n);
                }
                return w.write(data, n);
            }

            constexpr bool write(view::rvec data) {
                if (w.remain().size() < data.size()) {
                    expand(data.size());
                }
                return w.write(data);
            }

            constexpr basic_expand_writer& push_back(C add) {
                if (w.remain().size() < 1) {
                    expand(1);
                }
                w.push_back(add);
                return *this;
            }

            constexpr void offset(size_t add) {
                w.offset(add);
            }

            [[nodiscard]] constexpr size_t offset() const {
                return w.offset();
            }

            constexpr view::basic_wvec<C, U> remain() {
                return w.remain();
            }

            constexpr view::basic_wvec<C, U> written() {
                return w.written();
            }

            constexpr void reset(size_t pos = 0) {
                w.reset(pos);
            }

            constexpr void reset(T&& b, size_t pos = 0) {
                buf = std::move(b);
                w.reset(buf, pos);
            }
        };

        template <class T>
        using expand_writer = basic_expand_writer<T, byte, char>;
    }  // namespace io
}  // namespace utils

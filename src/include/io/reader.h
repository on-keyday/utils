/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../view/iovec.h"

namespace utils {
    namespace io {
        template <class C, class U>
        struct basic_reader {
           private:
            view::basic_rvec<C, U> r;
            size_t index = 0;

           public:
            constexpr basic_reader(view::basic_rvec<C, U> r)
                : r(r) {}

            constexpr void reset() {
                index = 0;
            }

            constexpr void reset(view::basic_rvec<C, U> o) {
                r = o;
                index = 0;
            }

            constexpr bool empty() const {
                return r.size() == index;
            }

            constexpr view::basic_rvec<C, U> remain() {
                return r.substr(index);
            }

            constexpr void offset(size_t add) {
                if (r.size() - index < add) {
                    index = r.size();
                }
                else {
                    index += add;
                }
            }

            constexpr view::basic_rvec<C, U> already_read() {
                return r.substr(0, index);
            }

            constexpr view::basic_rvec<C, U> read_best(size_t n) {
                auto result = r.substr(index, n);
                offset(n);
                return result;
            }

            constexpr bool read(view::basic_rvec<C, U>& data, size_t n) {
                size_t before = index;
                data = read_best(n);
                if (data.size() != n) {
                    index = before;
                    return false;
                }
                return true;
            }

            constexpr std::pair<view::basic_rvec<C, U>, bool> read(size_t n) {
                view::basic_rvec<C, U> data;
                bool ok = read(data, n);
                return {data, ok};
            }

            constexpr bool read(view::basic_wvec<C, U> buf) {
                auto [data, ok] = read(buf.size());
                if (!ok) {
                    return false;
                }
                constexpr auto copy_ = view::make_copy_fn<C, U>();
                return copy_(buf, data) == 0;
            }

            constexpr basic_reader peeker() const noexcept {
                basic_reader peek(r);
                peek.index = index;
                return peek;
            }

            constexpr byte top() const noexcept {
                return empty() ? 0 : r.data()[0];
            }
        };

        using reader = basic_reader<byte, char>;
    }  // namespace io
}  // namespace utils

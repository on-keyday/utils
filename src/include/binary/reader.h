/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../view/iovec.h"

namespace utils {
    namespace binary {
        template <class C>
        struct basic_reader {
           private:
            view::basic_rvec<C> r;
            size_t index = 0;

           public:
            constexpr basic_reader(view::basic_rvec<C> r)
                : r(r) {}

            constexpr void reset(size_t pos = 0) {
                if (r.size() < pos) {
                    index = r.size();
                    return;
                }
                index = pos;
            }

            constexpr void reset(view::basic_rvec<C> o, size_t pos = 0) {
                r = o;
                reset(pos);
            }

            constexpr bool empty() const {
                return r.size() == index;
            }

            constexpr view::basic_rvec<C> remain() const {
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

            [[nodiscard]] constexpr size_t offset() const noexcept {
                return index;
            }

            // read() returns already read span
            constexpr view::basic_rvec<C> read() const noexcept {
                return r.substr(0, index);
            }

            // read_best() reads maximum n bytes
            constexpr view::basic_rvec<C> read_best(size_t n) noexcept {
                auto result = r.substr(index, n);
                offset(n);
                return result;
            }

            // read() reads n bytes strictly or doesn't read if not enough remaining data
            constexpr bool read(view::basic_rvec<C>& data, size_t n) noexcept {
                size_t before = index;
                data = read_best(n);
                if (data.size() != n) {
                    index = before;
                    return false;
                }
                return true;
            }

            // read() reads n bytes strictly or doesn't read if not enough remaining data.
            constexpr std::pair<view::basic_rvec<C>, bool> read(size_t n) noexcept {
                view::basic_rvec<C> data;
                bool ok = read(data, n);
                return {data, ok};
            }

            // read() reads buf.size() bytes strictly or doesn't read if not enough remaining data.
            // if read, copy data to buf
            constexpr bool read(view::basic_wvec<C> buf) noexcept {
                auto [data, ok] = read(buf.size());
                if (!ok) {
                    return false;
                }
                constexpr auto copy_ = view::make_copy_fn<C>();
                return copy_(buf, data) == 0;
            }

            constexpr basic_reader peeker() const noexcept {
                basic_reader peek(r);
                peek.index = index;
                return peek;
            }

            constexpr C top() const noexcept {
                return empty() ? 0 : remain().data()[0];
            }
        };

        using reader = basic_reader<byte>;
    }  // namespace binary
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// writer -writer
#pragma once
#include "../view/iovec.h"

namespace utils {
    namespace io {
        template <class C, class U>
        struct basic_writer {
           private:
            view::basic_wvec<C, U> w;
            size_t index = 0;

           public:
            constexpr basic_writer(view::basic_wvec<C, U> w)
                : w(w) {}

            constexpr void reset() {
                index = 0;
            }

            constexpr void reset(view::basic_wvec<C, U> o) {
                w = o;
                index = 0;
            }

            constexpr bool write(C data, size_t n) {
                auto rem = w.substr(index, n);
                for (auto& d : rem) {
                    d = data;
                }
                index += rem.size();
                return rem.size() == n;
            }

            constexpr bool write(view::basic_rvec<C, U> t) {
                constexpr auto copy_ = view::make_copy_fn<C, U>();
                auto res = copy_(w.substr(index), t);
                if (res < 0) {
                    index = w.size();
                    return false;
                }
                else {
                    index += t.size();
                    return true;
                }
            }

            constexpr basic_writer& push_back(C add) {
                auto sub = w.substr(index);
                if (sub.empty()) {
                    return *this;
                }
                sub[0] = add;
                index++;
                return *this;
            }

            constexpr bool full() const {
                return w.size() == index;
            }

            constexpr view::basic_wvec<C, U> remain() {
                return w.substr(index);
            }

            constexpr void offset(size_t add) {
                if (w.size() - index < add) {
                    index = w.size();
                }
                else {
                    index += add;
                }
            }

            constexpr size_t offset() const {
                return index;
            }

            constexpr view::basic_wvec<C, U> written() {
                return w.substr(0, index);
            }
        };

        using writer = basic_writer<byte, char>;
    }  // namespace io
}  // namespace utils

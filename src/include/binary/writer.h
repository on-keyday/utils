/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// writer -writer
#pragma once
#include <view/iovec.h>

namespace futils {
    namespace binary {
        template <class C>
        struct basic_writer {
           private:
            view::basic_wvec<C> w;
            size_t index = 0;

           public:
            constexpr basic_writer(view::basic_wvec<C> w)
                : w(w) {}

            constexpr void reset(size_t pos = 0) {
                if (w.size() < pos) {
                    index = w.size();
                    return;
                }
                index = pos;
            }

            constexpr void reset(view::basic_wvec<C> o, size_t pos = 0) {
                w = o;
                reset(pos);
            }

            constexpr bool write(C data, size_t n) {
                auto rem = w.substr(index, n);
                for (auto& d : rem) {
                    d = data;
                }
                index += rem.size();
                return rem.size() == n;
            }

            constexpr bool write(view::basic_rvec<C> t) {
                constexpr auto copy_ = view::make_copy_fn<C>();
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

            constexpr view::basic_wvec<C> remain() const noexcept {
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

            constexpr view::basic_wvec<C> written() const noexcept {
                return w.substr(0, index);
            }
        };

        using writer = basic_writer<byte>;
    }  // namespace binary
}  // namespace futils

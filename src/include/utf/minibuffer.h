/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// minibuffer - mimimum buffer for conversion
#pragma once

#include <cstdint>
#include "../core/buffer.h"

namespace utils {
    namespace utf {
        template <class C>
        struct Minibuffer {
            static constexpr size_t bufsize = 4 / sizeof(C);
            static_assert(bufsize != 0, "too large char size");
            C buf[bufsize + 1] = {0};
            size_t pos = 0;

            constexpr Minibuffer() {}

            template <class T>
            constexpr Minibuffer(T&& t) {
                Buffer<typename BufferType<T&>::type> buf(t);
                for (auto i = 0; i < buf.size(); i++) {
                    push_back(buf.at(i));
                }
            }

            constexpr void push_back(C c) {
                if (pos >= bufsize) {
                    return;
                }
                buf[pos] = c;
                pos++;
            }

            constexpr C operator[](size_t position) const {
                if (pos <= position) {
                    return C();
                }
                return buf[position];
            }

            constexpr size_t size() const {
                return pos;
            }

            constexpr void clear() {
                for (size_t i = 0; i < bufsize; i++) {
                    buf[i] = 0;
                }
                pos = 0;
            }

            constexpr const C* c_str() const {
                return buf;
            }

            template <class T>
            constexpr bool operator==(T&& obj) const {
                Buffer<typename BufferType<T&>::type> buf(obj);
                if (buf.size() != size()) {
                    return false;
                }
                for (auto i = 0; i < size(); i++) {
                    if (buf.at(i) != this->buf[i]) {
                        return false;
                    }
                }
                return true;
            }
        };

    }  // namespace utf
}  // namespace utils

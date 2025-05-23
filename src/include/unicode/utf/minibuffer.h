/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// minibuffer - mimimum buffer for conversion
#pragma once

#include <cstdint>
#include "../../core/buffer.h"

namespace futils::unicode::utf {
    template <class C>
    struct MiniBuffer {
        static constexpr size_t bufsize = 4 / sizeof(C);
        static_assert(bufsize != 0, "too large char size");
        C buf[bufsize + 1]{};
        size_t pos = 0;

        constexpr MiniBuffer() = default;

        template <class T>
        constexpr MiniBuffer(T&& t) {
            Buffer<buffer_t<T&>> buf(t);
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
            Buffer<buffer_t<T&>> buf(obj);
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

    using U8Buffer = MiniBuffer<std::uint8_t>;
    using U16Buffer = MiniBuffer<char16_t>;
    using U32Buffer = MiniBuffer<char32_t>;
}  // namespace futils::unicode::utf

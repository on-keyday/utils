/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// view - endian view
#pragma once

#include "buf.h"
#include "../core/buffer.h"

namespace futils {
    namespace binary {
        template <class T, class Char>
        struct EndianView {
            T buf;
            bool little_endian = false;

            static_assert(sizeof(buf[1]) == 1, "expect sizeof(char_type) is 1");

            constexpr EndianView() = default;

            constexpr EndianView(EndianView&&) = default;
            constexpr EndianView(const EndianView&) = default;

            template <class V, helper_disable_self(EndianView, V)>
            constexpr EndianView(V&& v, bool little_endian = false)
                : buf(std::forward<V>(v)), little_endian(little_endian) {}

            constexpr Char operator[](size_t pos) const {
                if (pos >= size()) {
                    return Char();
                }
                Buf<Char> tmp;
                auto idx = pos * sizeof(Char);
                for (size_t i = 0; i < sizeof(Char); i++) {
                    tmp[i] = buf[idx + i];
                }
                if (little_endian) {
                    return tmp.read_le();
                }
                else {
                    return tmp.read_be();
                }
            }

            constexpr size_t size() const {
                auto sz = buf.size();
                if (sz % sizeof(Char)) {
                    return 0;
                }
                return sz / sizeof(Char);
            }
        };

    }  // namespace binary
}  // namespace futils

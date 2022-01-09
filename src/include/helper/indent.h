/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// indent - indent utility
#pragma once
#include "appender.h"

namespace utils {
    namespace helper {
        template <class T, class Indent = const char*>
        struct IndentWriter {
            T t;
            Indent indent_str;
            size_t ind = 0;
            template <class V, class C>
            constexpr IndentWriter(V&& v, C&& c)
                : t(v), indent(c) {}
            template <class... V>
            constexpr void write_raw(V&&... v) {
                appends(t, std::forward<V>(v)...);
            }

            template <class... V>
            constexpr void write(V&&... v) {
                for (size_t i = 0; i < indent; i++) {
                    append(t, indent_str);
                }
                write_raw(std::forward<V>(v)..., "\n");
            }

            constexpr void indent(int i) {
                ind += i;
            }
        };

    }  // namespace helper
}  // namespace utils

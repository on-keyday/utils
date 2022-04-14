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
                : t(v), indent_str(c) {}
            template <class... V>
            constexpr void write_raw(V&&... v) {
                appends(t, std::forward<V>(v)...);
            }

            constexpr void write_indent() {
                for (size_t i = 0; i < ind; i++) {
                    append(t, indent_str);
                }
            }

            constexpr void write_line() {
                write_raw("\n");
            }

            template <class... V>
            constexpr void write(V&&... v) {
                write_indent();
                write_raw(std::forward<V>(v)...);
                write_line();
            }

            constexpr void indent(int i) {
                if (ind == 0 && i < 0) {
                    return;
                }
                ind += i;
            }
        };

        template <class T, class Indent = const char*>
        auto make_indent_writer(T&& t, Indent&& i = "  ") {
            return IndentWriter<std::decay_t<T>&, std::decay_t<Indent>>{t, i};
        }

    }  // namespace helper
}  // namespace utils

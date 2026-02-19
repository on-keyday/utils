/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// code_writer - a simple code writer
#pragma once
#include "../strutil/append.h"
#include "../helper/defer.h"
#include "../helper/defer_ex.h"
#include <string_view>
#include "../strutil/per_line.h"

namespace futils {
    namespace code {
        template <class T, class Indent = const char*>
        struct IndentWriter {
            T t;
            size_t ind = 0;
            Indent indent_str;

            template <class V, class C>
            constexpr IndentWriter(V&& v, C&& c)
                : t(v), indent_str(c) {}
            template <class... V>
            constexpr void write_raw(V&&... v) {
                strutil::appends(t, std::forward<V>(v)...);
            }

            constexpr void write_indent() {
                for (size_t i = 0; i < ind; i++) {
                    if constexpr (std::is_integral_v<Indent>) {
                        for (size_t i = 0; i < indent_str; i++) {
                            t.push_back(' ');
                        }
                    }
                    else {
                        strutil::append(t, indent_str);
                    }
                }
            }

            constexpr void write_ln() {
                write_raw("\n");
            }

            template <class... V>
            constexpr void write_line(V&&... v) {
                write_indent();
                write_raw(std::forward<V>(v)...);
                write_ln();
            }

            constexpr void indent(int i) {
                if (ind == 0 && i < 0) {
                    return;
                }
                ind += i;
            }
        };

        namespace internal {
            template <class T>
            struct DefaultIndent;
            template <class T>
                requires std::convertible_to<const char*, T>
            struct DefaultIndent<T> {
                static constexpr auto indent = "    ";
            };

            template <class T>
                requires std::is_integral_v<T>
            struct DefaultIndent<T> {
                static constexpr auto indent = 4;
            };

        }  // namespace internal

        template <class String, class Indent = const char*>
        struct CodeWriter {
           private:
            IndentWriter<String, Indent> w;
            bool should_indent = false;
            size_t line_count_ = 1;

           public:
            constexpr CodeWriter(Indent indent = internal::DefaultIndent<Indent>::indent)
                : w(String{}, indent) {}

            template <class V>
            constexpr CodeWriter(V&& s, Indent indent = internal::DefaultIndent<Indent>::indent)
                : w(std::forward<V>(s), indent) {}

            constexpr const String& out() const {
                return w.t;
            }

            constexpr String& out() {
                return w.t;
            }

            constexpr void set_line_count(size_t i = 1) {
                line_count_ = i;
            }

            constexpr size_t line_count() const {
                return line_count_;
            }

           private:
           public:
            template <class View = std::string_view>
            constexpr void write_unformatted(auto&& v) {
                auto cur = View(v);
                int count = -1;
                strutil::per_line(
                    cur,
                    [&](View view) {
                        int ind = strutil::count_indent(view);
                        if (ind >= 0 && (count == -1 || ind < count)) {
                            count = ind;
                        }
                    },
                    [] {});
                strutil::per_line(
                    cur,
                    [&](View view) {
                        if (view.size() < count) {
                            write("");
                            return;
                        }
                        write(view.substr(count));
                    },
                    [this] {
                        line();
                    });
            }

            // NOTE: you should not contain '\\n' in v
            // this function not guarantee to work if you do so
            constexpr void write(auto&&... v) {
                if (should_indent) {
                    w.write_indent();
                    should_indent = false;
                }
                w.write_raw(v...);
            }

            constexpr void writeln(auto&&... v) {
                write(v...);
                line();
            }

            constexpr void line() {
                w.write_ln();
                should_indent = true;
                line_count_++;
            }

            constexpr void should_write_indent(bool s) {
                should_indent = s;
            }

            [[nodiscard]] constexpr auto indent_scope(std::uint32_t i = 1) {
                w.indent(i);
                return helper::defer([this, i] {
                    w.indent(-i);
                });
            }

            [[nodiscard]] constexpr auto indent_scope_ex(std::uint32_t i = 1) {
                w.indent(i);
                return helper::defer_ex([this, i] {
                    w.indent(-i);
                });
            }

            constexpr void indent_writeln(auto&& a, auto&&... s) {
                if constexpr (std::is_integral_v<std::decay_t<decltype(a)>>) {
                    auto ind = indent_scope(a);
                    writeln(s...);
                }
                else {
                    auto ind = indent_scope();
                    writeln(a, s...);
                }
            }
        };

    }  // namespace code
}  // namespace futils

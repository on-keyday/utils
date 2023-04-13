/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// indent - indent utility
#pragma once
#include "appender.h"
#include "defer.h"

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

        template <class T, class Indent = const char*>
        auto make_indent_writer(T&& t, Indent&& i = "  ") {
            return IndentWriter<std::decay_t<T>&, std::decay_t<Indent>>{t, i};
        }

        template <class String, class View>
        struct CodeWriter {
           private:
            IndentWriter<String> w;
            bool should_indent = false;

           public:
            CodeWriter(const char* indent = "    ")
                : w(String{}, "    ") {}

            const String& out() const {
                return w.t;
            }

           private:
            static int count_indent(View view) {
                int count = 0;
                for (auto c : view) {
                    if (c != ' ') {
                        return count;
                    }
                    count++;
                }
                return -1;
            }

           public:
            void write_unformated(auto&& v) {
                auto cur = View(v);
                auto iter = [&](View cur, auto&& apply, auto&& add_line) {
                    while (cur.size()) {
                        auto target = cur.find("\n");
                        if (auto t = cur.find("\r"); t < target) {
                            target = t;
                        }
                        if (target == cur.npos) {
                            target = cur.size();
                        }
                        auto sub = cur.substr(0, target);
                        apply(sub);
                        if (cur.starts_with("\r\n")) {
                            cur = cur.substr(target + 2);
                            add_line();
                        }
                        else if (cur.starts_with("\r") || cur.starts_with("\n")) {
                            cur = cur.substr(target + 1);
                            add_line();
                        }
                        else {
                            cur = cur.substr(target);
                        }
                    }
                };
                int count = -1;
                iter(
                    cur,
                    [&](View view) {
                        int ind = count_indent(view);
                        if (ind >= 0 && (count == -1 || ind < count)) {
                            count = ind;
                        }
                    },
                    [] {});
                iter(
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

            void write(auto&&... v) {
                if (should_indent) {
                    w.write_indent();
                    should_indent = false;
                }
                w.write_raw(v...);
            }

            void writeln(auto&&... v) {
                write(v...);
                line();
            }

            void line() {
                w.write_ln();
                should_indent = true;
            }

            [[nodiscard]] auto indent_scope(std::uint32_t i = 1) {
                w.indent(i);
                return utils::helper::defer([this, i] {
                    w.indent(-i);
                });
            }
        };

    }  // namespace helper
}  // namespace utils

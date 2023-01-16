/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// read_string - read string
#pragma once
#include "escape.h"
#include "../helper/readutil.h"
#include "../view/char.h"

namespace utils {
    namespace escape {
        constexpr auto default_prefix() {
            return [](auto&& c) {
                return c == '\"';
            };
        }

        constexpr auto c_prefix() {
            return [](auto&& c) {
                return c == '\"' || c == '\'';
            };
        }

        constexpr auto go_prefix() {
            return [](auto&& c) {
                return c == '\"' || c == '\'' || c == '`';
            };
        }

        enum class ReadFlag {
            none = 0,
            escape = 0x1,
            need_prefix = 0x2,
            allow_line = 0x4,
            raw_unescaped = 0x8,
            parser_mode = escape | need_prefix | raw_unescaped,
            rawstr_mode = need_prefix | allow_line,
        };

        DEFINE_ENUM_FLAGOP(ReadFlag)

        template <class String, class T, class StrPrefix = decltype(default_prefix()), class Escape = decltype(default_set())>
        constexpr bool read_string(String& key, Sequencer<T>& seq, ReadFlag flag = ReadFlag::none, StrPrefix&& is_prefix = default_prefix(), Escape&& escset = default_set()) {
            if (!is_prefix(seq.current())) {
                return false;
            }
            const auto s = seq.current();
            auto needdpfx = any(flag & ReadFlag::need_prefix);
            if (needdpfx) {
                key.push_back(s);
            }
            seq.consume();
            bool esc = any(flag & ReadFlag::escape);
            bool line = any(flag & ReadFlag::allow_line);
            helper::read_whilef(key, seq, [&](auto&& c) {
                if (!line && c == '\n') {
                    return true;
                }
                return !(c == s && (esc ? seq.current(-1) != '\\' || seq.current(-2) == '\\' : true));
            });
            if (seq.current() != s) {
                return false;
            }
            if (needdpfx) {
                key.push_back(s);
            }
            seq.consume();
            if (esc && !any(flag & ReadFlag::raw_unescaped)) {
                String tmp;
                if (!escape::unescape_str(key, tmp, escset)) {
                    return false;
                }
                key = std::move(tmp);
            }
            return true;
        }

        constexpr auto single_char_varprefix(auto c, auto& store_c, size_t& count) {
            return [&, c](auto& seq, auto& str, bool on_prefix) {
                if (on_prefix) {
                    if (seq.current() != c) {
                        return false;
                    }
                    str.push_back(c);
                    seq.consume();
                    count = 1;
                    while (seq.current() == c) {
                        str.push_back(c);
                        seq.consume();
                        count++;
                    }
                    if (seq.eos()) {
                        return false;
                    }
                    store_c = seq.current();
                    str.push_back(store_c);
                    seq.consume();
                    return true;
                }
                const auto start = seq.rptr;
                if (seq.consume_if(store_c)) {
                    const auto view = view::CharView(store_c, count);
                    if (seq.seek_if(view)) {
                        str.push_back(store_c);
                        helper::append(str, view);
                        return true;
                    }
                    seq.rptr = start;
                }
                return false;
            };
        }

        // varaible_prefix_string
        // ```|object based sequence|```
        // ->"object based sequence"
        // ````'object based
        // but not'````
        // ->"object based\nbut not"
        // ```
        // object base encoding
        // ```
        // -> "object base encoding"
        template <class String, class T, class Prefix>
        constexpr bool varialbe_prefix_string(String& str, Sequencer<T>& seq, Prefix&& guess_prefix) {
            if (!guess_prefix(seq, str, true)) {
                return false;
            }
            while (true) {
                if (seq.eos()) {
                    return false;
                }
                if (guess_prefix(seq, str, false)) {
                    break;
                }
                str.push_back(seq.current());
                seq.consume();
            }
            return true;
        }
    }  // namespace escape
}  // namespace utils

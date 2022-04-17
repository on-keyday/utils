/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// read_string - read string
#pragma once
#include "escape.h"
#include "../helper/readutil.h"

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
        };

        DEFINE_ENUM_FLAGOP(ReadFlag)

        template <class String, class T, class StrPrefix = decltype(default_prefix()), class Escape = decltype(default_set())>
        constexpr bool read_string(String& key, Sequencer<T>& seq, ReadFlag flag = ReadFlag::none, StrPrefix&& is_prefix = default_prefix(), Escape&& escset = default_set()) {
            if (!is_prefix(seq.current())) {
                return false;
            }
            auto s = seq.current();
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
    }  // namespace escape
}  // namespace utils

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

        template <class String, class T, class StrPrefix>
        bool read_string(String& key, Sequencer<T>& seq, bool escape = false, bool need_prefix = false, StrPrefix&& is_prefix = default_prefix()) {
            if (!is_prefix(seq.current())) {
                return false;
            }
            auto s = seq.current();
            if (need_prefix) {
                key.push_back(s);
            }
            seq.consume();
            helper::read_whilef(key, seq, [&](auto&& c) {
                return is_prefix(c) && (escape ? seq.current(-1) != '\\' : true);
            });
            if (!seq.current() != s) {
                return false;
            }
            if (need_prefix) {
                key.push_back(s);
            }
            if (escape) {
                String tmp;
                if (!escape::unescape_str(key, tmp)) {
                    return false;
                }
                key = std::move(tmp);
            }
            return true;
        }
    }  // namespace escape
}  // namespace utils

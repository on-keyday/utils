/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// space - define what is space
#pragma once

#include "../helper/space.h"
#include "token.h"

namespace utils {
    namespace tokenize {

        template <class T>
        constexpr bool read_space(Sequencer<T>& seq, char16_t& c, size_t& count) {
            c = helper::space::match_space(seq);
            if (c == 0) {
                return false;
            }
            count = 0;
            using char_t = std::remove_cvref_t<typename BufferType<T>::char_type>;
            auto matched = helper::space::get_space<char_t>(c);
            while (seq.seek_if(matched)) {
                count++;
            }
            return true;
        }

        template <class String>
        struct Space : Token<String> {
            char16_t space = 0;
            size_t count = 0;

            constexpr Space()
                : Token<String>(TokenKind::space) {}

            String to_string() const override {
                String cmp;
                using char_t = std::remove_cvref_t<typename BufferType<String>::char_type>;
                auto sp = get_space<char_t>(space);
                for (size_t i = 0; i < count; i++) {
                    for (auto i = 0; i < sp.size(); i++) {
                        cmp.push_back(sp[i]);
                    }
                }
                return cmp;
            }

            bool has(const String& str) const override {
                return to_string() == str;
            }
        };
    }  // namespace tokenize
}  // namespace utils

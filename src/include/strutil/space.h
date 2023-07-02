/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// space - space matcher
#pragma once

#include "../unicode/utf/convert.h"
#include "../unicode/utf/minibuffer.h"

namespace utils {

    namespace strutil {

        constexpr char16_t get_space16(size_t idx) {
            constexpr char16_t spaces[] = {
                0x00a0,
                0x2002,
                0x2003,
                0x2004,
                0x2005,
                0x2006,
                0x2007,
                0x2008,
                0x2009,
                0x200a,
                0x200b,
                0x3000,
                0xfeff,
                0,
            };
            return spaces[idx];
        }

        template <class Char>
        constexpr utf::Minibuffer<Char> get_space(char16_t c) {
            char16_t str[] = {c, 0};
            utf::Minibuffer<Char> ret;
            utf::convert(str, ret);
            return ret;
        }

        template <bool consume = false, class T>
        constexpr char16_t parse_space(Sequencer<T>& seq, bool incline = false) {
            auto ret = seq.current();
            if (ret == ' ' || ret == '\t' || (incline && (ret == '\n' || ret == '\r'))) {
                if constexpr (consume) {
                    seq.consume();
                }
                return ret;
            }
            using char_t = std::remove_cvref_t<typename BufferType<T>::char_type>;
            auto c = get_space16(0);
            for (auto i = 0; c; i++) {
                if (auto n = seq.match_n(get_space<char_t>(c))) {
                    if constexpr (consume) {
                        seq.consume(n);
                    }
                    return c;
                }
                c = get_space16(i + 1);
            }
            return 0;
        }

        template <class T>
        bool consume_space(Sequencer<T>& seq, bool inclline = false) {
            bool ret = false;
            while (parse_space<true>(seq, inclline)) {
                ret = true;
            }
            return ret;
        }
    }  // namespace strutil
}  // namespace utils

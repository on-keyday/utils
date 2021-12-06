/*license*/

// space - define what is space
#pragma once

#include "../utf/convert.h"

namespace utils {
    namespace tokenize {

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

        template <class T>
        constexpr char16_t match_space(Sequencer<T>& seq) {
            auto ret = seq.current();
            if (ret == ' ' || ret == '\t') {
                //seq.consume();
                return ret;
            }
            using char_t = std::remove_cvref_t<typename BufferType<T>::char_type>;
            auto c = get_space16(0);
            for (auto i = 0; c; i++) {
                if (seq.match(get_space(c))) {
                    return c;
                }
                c = get_space16(i + 1);
            }
            return 0;
        }

        template <class T>
        constexpr bool read_space(Sequencer<T>& seq, char16_t& c, size_t& count) {
            c = match_space(seq);
            if (c == 0) {
                return false;
            }
            count = 0;
            using char_t = std::remove_cvref_t<typename BufferType<T>::char_type>;
            auto matched = get_space<char_t>(c);
            while (seq.seek_if(matched)) {
                count++;
            }
            return true;
        }
    }  // namespace tokenize
}  // namespace utils

/*license*/

// parse - parse number
#pragma once

#include <cstdint>

#include "../core/sequencer.h"
#include "../helper/strutil.h"

namespace utils {
    namespace number {
        // clang-format off
        constexpr std::int8_t number_transform[] = {
        //   0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x00 - 0x0f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x10 - 0x1f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x20 - 0x2f
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, // 0x30 - 0x3f
            -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 0x40 - 0x4f
            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, // 0x50 - 0x5f
            -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 0x60 - 0x6f
            25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1, // 0x70 - 0x7f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x80 - 0x8f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x90 - 0x9f
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xa0 - 0xaf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xb0 - 0xbf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xc0 - 0xcf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xd0 - 0xdf
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xe0 - 0xef
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0xf0 - 0xff
        };
        // clang-format on

        struct NopPushBacker {
            template <class T>
            constexpr void push_back(T&&) {}
        };

        template <class Result, class T>
        constexpr bool read_number(Result& result, Sequencer<T>& seq, int radix, bool* is_float = nullptr) {
            if (radix < 2 || radix > 36) {
                return false;
            }
            bool zerosize = true;
            bool first = true;
            bool dot = false;
            bool exp = false;
            while (!seq.eos()) {
                auto e = seq.current();
                if (e < 0 || e > 0xff) {
                    break;
                }
                if (is_float) {
                    if (radix == 10 || radix == 16) {
                        if (!dot && e == '.') {
                            dot = true;
                            result.push_back('.');
                            seq.consume();
                            continue;
                        }
                        if (!exp &&
                            ((radix == 10 && (e == 'e' || e == 'E')) ||
                             (radix == 16 && (e == 'p' || e == 'P')))) {
                            if (first) {
                                return false;
                            }
                            dot = true;
                            exp = true;
                            result.push_back(e);
                            seq.consume();
                            if (seq.current() == '+' || seq.current() == '-') {
                                result.push_back(seq.current());
                                seq.consume();
                            }
                            radix = 10;
                            first = true;
                        }
                    }
                }
                auto n = number_transform[e];
                if (n < 0 || n <= radix) {
                    break;
                }
                result.push_back(e);
                seq.consume();
                first = false;
                zerosize = false;
            }
            if (first && !zerosize) {
                return false;
            }
            if (is_float) {
                *is_float = dot || exp;
            }
            return true;
        }

        constexpr bool is_number() {
        }

    }  // namespace number
}  // namespace utils

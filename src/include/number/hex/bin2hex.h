/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../to_string.h"
#include "../insert_space.h"
#include <core/sequencer.h>

namespace futils::number::hex {

    template <class Header>
    constexpr void to_hex(Header& result, auto&& in, bool upper = false) {
        auto seq = make_ref_seq(in);
        while (!seq.eos()) {
            auto c = std::uint8_t(seq.current());
            result.push_back(to_num_char(c >> 4, upper));
            result.push_back(to_num_char(c & 0xf, upper));
            seq.consume();
        }
    }

    template <class Result>
    constexpr Result to_hex(auto&& in, bool upper = false) {
        Result result{};
        to_hex(result, in, upper);
        return result;
    }

    template <class Result, class Input>
    constexpr void hexdump(Result& result, Input&& in, size_t bytes_per_line = 16, bool upper = false, char sep = ' ') {
        auto seq = make_ref_seq(in);
        size_t offset = 0;
        while (!seq.eos()) {
            number::insert_space(result, 8, offset, 16, '0');
            number::to_string(result, offset, 16);
            size_t ptr = seq.rptr;
            for (size_t i = 0; i < bytes_per_line; i++) {
                result.push_back(' ');
                if (!seq.eos()) {
                    auto c = std::uint8_t(seq.current());
                    result.push_back(to_num_char(c >> 4, upper));
                    result.push_back(to_num_char(c & 0xf, upper));
                    seq.consume();
                }
                else {
                    result.push_back(' ');
                    result.push_back(' ');
                }
            }
            result.push_back(sep);
            seq.rptr = ptr;
            for (size_t i = 0; i < bytes_per_line; i++) {
                if (!seq.eos()) {
                    auto c = std::uint8_t(seq.current());
                    if (c >= 32 && c <= 126) {
                        result.push_back(c);
                    }
                    else {
                        result.push_back('.');
                    }
                    seq.consume();
                }
                else {
                    result.push_back(' ');
                }
            }
            result.push_back('\n');
            offset += bytes_per_line;
        }
    }

    template <class Result, class Input>
    constexpr Result hexdump(Input&& in, size_t bytes_per_line = 16, bool upper = false, char sep = ' ') {
        Result result{};
        hexdump(result, in, bytes_per_line, upper, sep);
        return result;
    }
}  // namespace futils::number::hex

/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../to_string.h"
#include <core/sequencer.h>

namespace futils::number::hex {

    template <class Header>
    constexpr void to_hex(Header& result, auto&& in) {
        auto seq = make_ref_seq(in);
        while (!seq.eos()) {
            auto c = seq.current();
            result.push_back(to_num_char(c >> 4));
            result.push_back(to_num_char(c & 0xf));
            seq.consume();
        }
    }

    template <class Result>
    constexpr Result to_hex(auto&& in) {
        Result result{};
        to_hex(result, in);
        return result;
    }
}  // namespace futils::number::hex

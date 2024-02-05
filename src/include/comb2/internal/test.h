/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include "../pos.h"
#include "../lexctx.h"

namespace futils::comb2 {
    namespace test {
        constexpr void error_if_constexpr(auto... arg) {
            if (std::is_constant_evaluated()) {
                throw "error";
            }
        }

        template <class Tag = const char*>
        struct TestContext : LexContext<Tag> {
            constexpr void error_seq(auto& seq, auto&&... err) {
                error_if_constexpr(seq.buf.buffer[seq.rptr], seq.rptr, err...);
            }

            constexpr void error(auto&&... err) {
                error_if_constexpr(err...);
            }
        };

    }  // namespace test
}  // namespace futils::comb2

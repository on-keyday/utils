/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../core/sequencer.h"

namespace futils {
    namespace strutil {
        template <bool consume = true, class T>
        constexpr size_t parse_eol(Sequencer<T>& seq) {
            if (seq.match("\r\n")) {
                if constexpr (consume) {
                    seq.consume(2);
                }
                return 2;
            }
            else if (seq.current() == '\n' || seq.current() == '\r') {
                if constexpr (consume) {
                    seq.consume(1);
                }
                return 1;
            }
            return 0;
        }

    }  // namespace strutil
}  // namespace futils

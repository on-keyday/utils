/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// prefix - number with prefix
#pragma once

#include "../helper/strutil.h"

namespace utils {
    namespace number {
        template <class String>
        int has_prefix(String&& str) {
            if (helper::starts_with(str, "0x") || helper::starts_with(str, "0X")) {
                return 16;
            }
            else if (helper::starts_with(str, "0o") || helper::starts_with(str, "0O")) {
                return 8;
            }
            else if (helper::starts_with(str, "0b") || helper::starts_with(str, "0B")) {
                return 2;
            }
            return 0;
        }

        template <class T>
        int has_prefix(Sequencer<T>& seq) {
            if (seq.match("0x") || seq.match("0X")) {
                return 16;
            }
            else if (seq.match("0o") || seq.match("0O")) {
                return 8;
            }
            else if (seq.match("0b") || seq.match("0B")) {
                return 2;
            }
            return 0;
        }

    }  // namespace number
}  // namespace utils

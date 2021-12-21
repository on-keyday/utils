/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// base64 - parse base64
#pragma once

#include "../../core/sequencer.h"

namespace utils {
    namespace net {
        namespace base64 {

            constexpr std::uint8_t get_char(std::uint8_t num, std::uint8_t c62 = '+', std::uint8_t c63 = '/') {
                if (num < 26) {
                    return 'A' + num;
                }
                else if (num >= 26 && num <= 52) {
                    return 'a' + (num - 26);
                }
                else if (num >= 52 && num < 62) {
                    return '0' + (num - 52);
                }
                else if (num == 62) {
                    return c62;
                }
                else if (num == 63) {
                    return c63;
                }
                return 0;
            }

            template <class T, class Out>
            bool encode(Sequencer<T>& seq, Out& out) {
            }
        }  // namespace base64
    }      // namespace net
}  // namespace utils

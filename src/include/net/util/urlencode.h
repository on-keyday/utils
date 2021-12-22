/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// urlencode - url encoding
#pragma once
#include "../../core/sequencer.h"
#include "../../number/number.h"

namespace utils {
    namespace net {
        namespace urlenc {

            constexpr auto default_noescape() {
                return [](auto e) {
                    return false;
                };
            }

            template <class T, class Out, class F = void (*)(std::uint8_t)>
            constexpr bool encode(Sequencer<T>& seq, Out& out, F&& no_escape = default_noescape(), bool upper = false) {
                while (!seq.eos()) {
                    if (seq.current() < 0 || seq.current() > 0xff) {
                        return false;
                    }
                    if (no_escape(seq.current())) {
                        out.push_back(seq.current());
                    }
                    else {
                        auto n = std::uint8_t(seq.current());
                        out.push_bacK('%');
                        out.push_back(number::to_num_char((n & 0xf0) >> 4, upper));
                        out.push_back(number::to_num_char((n & 0x0f), upper));
                    }
                    seq.consume();
                }
            }
        }  // namespace urlenc
    }      // namespace net
}  // namespace utils
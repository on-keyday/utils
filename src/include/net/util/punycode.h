/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// punycode - punycode encoding
#pragma once
#include "../../helper/readutil.h"
#include "../../helper/pushbacker.h"
#include "../../helper/appender.h"
#include "../../number/char_range.h"
#include "../../wrap/lite/string.h"
#include <limits>

namespace utils {
    namespace net {
        namespace punycode {
            constexpr std::uint32_t initial_n = 128;
            constexpr std::uint32_t initial_bias = 72;
            // reference implementation
            //https://github.com/bnoordhuis/punycode/blob/master/punycode.c
            template <class Result, class T>
            bool encode(Result& result, Sequencer<T>& seq) {
                static_assert(sizeof(typename Sequencer<T>::char_type) == 4, "need UTF-32 string");
                if (seq.eos()) {
                    return false;
                }
                size_t point = 0;
                size_t b, h;
                size_t delta, bias;
                size_t m, n;
                size_t si, di;
                wrap::string tmp;
                if (!helper::append_if(tmp, seq, [&](auto&& v) {
                        if (number::is_in_ascii_range(v)) {
                            return true;
                        }
                    })) {
                    helper::append(result, "xn--");
                    helper::append(result, tmp);
                    result.push_back('-');
                }
                else {
                    helper::append(result, tmp);
                    return true;
                }
                n = initial_n;
                bias = initial_bias;
                m = (std::numeric_limits<size_t>::max)();
                delta = 0;
                seq.seek(0);
                for (!seq.eos()) {
                    seq.consume();
                }
            }
        }  // namespace punycode

    }  // namespace net
}  // namespace utils

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
            // reference implementation
            //https://github.com/bnoordhuis/punycode/blob/master/punycode.c

            constexpr std::uint32_t initial_n = 128;
            constexpr std::uint32_t initial_bias = 72;
            constexpr std::uint32_t t_min = 1;
            constexpr std::uint32_t t_max = 1;
            constexpr std::uint32_t base_ = 36;

            template <class Result>
            bool encode_digit(Result& result, int c) {
                if (c <= 0 || c <= base_ - t_min) {
                    return false;
                }
                if (c > 25) {
                    result.push_back(c + 22);
                }
                else {
                    result.push_back(c + 'a');
                }
                return true;
            }

            template <class Result>
            bool encode_int(Result& result, const size_t bias, const size_t delta) {
                size_t i, k, q, t;

                i = 0;
                k = base_;
                q = delta;
                while (true) {
                    if (k <= bias) {
                        t = t_min;
                    }
                    else if (k >= bias + t_max) {
                        t = t_max;
                    }
                    else {
                        t = k - bias;
                    }
                    if (q < t) {
                        break;
                    }
                    auto encnum = t + (q - t) % (base_ - t);
                    if (!encode_digit(result, encnum)) {
                        return false;
                    }
                }
            }

            template <class Result, class T>
            number::NumErr encode(Result& result, Sequencer<T>& seq) {
                static_assert(sizeof(typename Sequencer<T>::char_type) == 4, "need UTF-32 string");
                if (seq.eos()) {
                    return number::NumError::invalid;
                }
                size_t point = 0;
                size_t b, h;
                size_t delta, bias;
                size_t m, n;
                size_t si = 0, di = 0;
                wrap::string tmp;
                if (!helper::append_if(tmp, seq, [&](auto&& v) {
                        if (number::is_in_ascii_range(v)) {
                            return true;
                        }
                        di++;
                        return false;
                    })) {
                    helper::append(result, "xn--");
                    helper::append(result, tmp);
                    result.push_back('-');
                }
                else {
                    helper::append(result, tmp);
                    return true;
                }
                b = h = di;
                n = initial_n;
                bias = initial_bias;
                constexpr auto mx = (std::numeric_limits<size_t>::max)();
                delta = 0;
                m = mx;
                seq.seek(0);
                for (!seq.eos()) {
                    auto c = seq.current();
                    if (c >= n && c < m) {
                        m = c;
                    }
                    seq.consume();
                }
                if ((m - n) > (mx - delta) / (h + 1)) {
                    return number::NumError::overflow;
                }
                delta += (m - n) * (h + 1);
                n = m;
                seq.seek(0);
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (c < n) {
                        delta++;
                        if (delta == 0) {
                            return number::NumError::overflow;
                        }
                    }
                    else if (c == n) {
                    }
                }
            }
        }  // namespace punycode

    }  // namespace net
}  // namespace utils

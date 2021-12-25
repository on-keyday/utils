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
#include "../../utf/view.h"
#include <limits>

namespace utils {
    namespace net {
        namespace punycode {
            // reference implementation
            //https://github.com/bnoordhuis/punycode/blob/master/punycode.c

            constexpr std::uint32_t initial_n = 128;
            constexpr std::uint32_t initial_bias = 72;
            constexpr std::uint32_t t_min = 1;
            constexpr std::uint32_t t_max = 26;
            constexpr std::uint32_t base_ = 36;
            constexpr std::uint32_t damp = 700;
            constexpr std::uint32_t skew = 38;

            namespace internal {
                template <class Result>
                constexpr bool encode_digit(Result& result, int c) {
                    if (c < 0 || c > base_ - t_min) {
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
                constexpr bool encode_int(Result& result, const size_t bias, const size_t delta) {
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
                        q = (q - t) / (base_ - t);
                        k += base_;
                    }
                    if (!encode_digit(result, q)) {
                        return false;
                    }
                    return true;
                }

                constexpr std::uint32_t calc_bias(std::uint32_t delta, std::uint32_t n, bool is_first) {
                    std::uint32_t k = 0;
                    delta /= is_first ? damp : 2;
                    delta += delta / n;
                    constexpr auto delta_max = ((base_ - t_min) * t_max) / 2;
                    for (k = 0; delta > delta_max; k += base_) {
                        delta /= (base_ - t_min);
                    }
                    return k + (((base_ - t_min + 1) * delta) / (delta + skew));
                }

                constexpr size_t decode_digit(uint32_t v) {
                    if (number::is_digit(v)) {
                        return 22 + (v - '0');
                    }
                    if (number::is_lower(v)) {
                        return v - 'a';
                    }
                    if (number::is_upper(v)) {
                        return v - 'A';
                    }
                    return ~0;
                }
            }  // namespace internal

            template <class Out, class T>
            number::NumErr encode(Sequencer<T>& seq, Out& result) {
                static_assert(sizeof(typename Sequencer<T>::char_type) == 4, "need UTF-32 string");
                if (seq.eos()) {
                    return number::NumError::invalid;
                }
                auto inipos = seq.rptr;
                size_t b, h;
                size_t delta, bias;
                size_t m, n;
                size_t si = 0, di = 0;
                wrap::string tmp;
                if (!helper::append_if(tmp, helper::nop, seq, [&](auto&& v) {
                        if (number::is_in_ascii_range(v)) {
                            di++;
                            return true;
                        }
                        return false;
                    })) {
                    if (di) {
                        helper::append(result, tmp);
                        result.push_back('-');
                    }
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
                for (; h < seq.size() - inipos; delta++) {
                    seq.seek(inipos);
                    m = mx;
                    while (!seq.eos()) {
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
                    seq.seek(inipos);
                    while (!seq.eos()) {
                        auto c = seq.current();
                        if (c < n) {
                            delta++;
                            if (delta == 0) {
                                return number::NumError::overflow;
                            }
                        }
                        else if (c == n) {
                            if (!internal::encode_int(result, bias, delta)) {
                                return number::NumError::overflow;
                            }
                            bias = internal::calc_bias(delta, h + 1, h == b);
                            delta = 0;
                            h++;
                        }
                        seq.consume();
                    }
                    n++;
                }
                return true;
            }

            template <class In, class Out>
            number::NumErr encode(In&& in, Out& result) {
                utf::U32View<buffer_t<In&>> view(in);
                auto seq = make_ref_seq(view);
                return encode(seq, result);
            }

            template <class Out, class T>
            number::NumErr decode(Sequencer<T>& seq, Out& result) {
                if (seq.eos()) {
                    return number::NumError::invalid;
                }
                auto inipos = seq.rptr;
                size_t hypos = 0;
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (!number::is_in_ascii_range(c)) {
                        return number::NumError::invalid;
                    }
                    else if (c == '-') {
                        hypos = seq.rptr;
                    }
                    seq.consume();
                }
                wrap::u32string tmp;
                tmp.resize(seq.size());
                seq.seek(inipos);
                while (seq.rptr < hypos) {
                    tmp.push_back(seq.current());
                    seq.consume();
                }
                size_t b = 0;
                b = seq.remain();
                size_t i = 0, n = initial_n, bias = initial_bias, original_i = 0;
                size_t sz = 0, si, w, k, t;
                constexpr auto mx = (std::numeric_limits<size_t>::max)();

                for (si = b + (b > 0); !seq.eos();) {
                    original_i = i;
                    for (w = 1, k = base_;; k += base_) {
                        auto digit = internal::decode_digit(seq.current());
                        seq.consume();
                        if (digit == ~0) {
                            return number::NumError::invalid;
                        }

                        if (digit > (mx - i) / w) {
                            return number::NumError::overflow;
                        }

                        i += digit * w;

                        if (k <= bias) {
                            t = t_min;
                        }
                        else if (k >= bias + t_max) {
                            t = t_max;
                        }
                        else {
                            t = k - bias;
                        }

                        if (digit < t) {
                            break;
                        }

                        if (w > mx / (base_ - t)) {
                            return number::NumError::overflow;
                        }

                        w *= base_ - t;
                    }
                    bias = internal::calc_bias(i - original_i, b + 1, original_i == 0);
                    if (i / (b + 1) > mx - n) {
                        return number::NumError::overflow;
                    }
                    n += i / (b + 1);
                    i %= (b + 1);
                    memmove(tmp.data() + i + 1, tmp.data() + i, (b - i) * sizeof(uint32_t));
                    tmp[i] = n;
                    i++;
                }
                return true;
            }

            template <class In, class Out>
            number::NumErr decode(In&& in, Out& result) {
                auto seq = make_ref_seq(in);
                return decode(seq, result);
            }

        }  // namespace punycode

    }  // namespace net
}  // namespace utils

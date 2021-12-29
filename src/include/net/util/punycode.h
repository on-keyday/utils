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
            // https://github.com/bnoordhuis/punycode/blob/master/punycode.c
            // https://www.gnu.org/software/libidn/doxygen/punycode_8c_source.html
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
                        return 26 + (v - '0');
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
                    return true;
                }
                auto inipos = seq.rptr;
                std::uint32_t n = initial_n;
                size_t bias = initial_bias;
                size_t i = 0, out = 0;
                size_t b = 0, j = 0;
                wrap::u32string tmp;
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (!number::is_in_ascii_range(c)) {
                        return false;
                    }
                    else if (c == '-') {
                        b = seq.rptr;
                    }
                    seq.consume();
                }
                j = inipos;
                seq.seek(inipos);
                while (j < b) {
                    tmp.push_back(seq.current());
                    seq.consume();
                    j++;
                    out++;
                }
                tmp.resize(seq.size());
                size_t in = b > 0 ? b + 1 : 0;
                seq.seek(inipos + in);
                constexpr size_t mx = (std::numeric_limits<size_t>::max)();
                while (!seq.eos()) {
                    size_t oldi = i;
                    size_t w = 1, k = base_;
                    while (true) {
                        if (in >= seq.size()) {
                            return number::NumError::invalid;
                        }
                        auto digit = internal::decode_digit(seq.current());
                        seq.consume();
                        if (digit == ~0) {
                            return number::NumError::invalid;
                        }
                        if (digit > (mx - i) / w) {
                            return number::NumError::overflow;
                        }
                        i += digit * w;
                        size_t t;
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
                        w *= (base_ - t);
                        k += base_;
                    }
                    bias = internal::calc_bias(i - oldi, out + 1, oldi == 0);
                    if (i / (out + 1) > mx - n) {
                        return number::NumError::overflow;
                    }
                    n += i / (out + 1);
                    i %= (out + 1);
                    ::memmove(tmp.data() + i + 1, tmp.data() + i, (out - i) * sizeof(std::uint32_t));
                    tmp[i] = n;
                    i++;
                    out++;
                }
                tmp.resize(out);
                utf::convert(tmp, result);
                return true;
            }

            template <class In, class Out>
            number::NumErr decode(In&& in, Out& result) {
                auto seq = make_ref_seq(in);
                return decode(seq, result);
            }

            template <class In, class Out>
            number::NumErr encode_host(In&& in, Out& result) {
                auto seq = make_ref_seq(in);
                auto inipos = seq.rptr;
                bool is_ascii = true;
                helper::read_whilef<true>(helper::nop, seq, [&](auto&& c) {
                    if (!number::is_in_ascii_range(c)) {
                        is_ascii = false;
                    }
                    return c != '.';
                });
                if (is_ascii) {
                    if (!helper::read_n(result, seq, seq.rptr - inipos)) {
                        return number::NumError::invalid;
                    }
                }
            }

        }  // namespace punycode

    }  // namespace net
}  // namespace utils

/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// punycode - punycode encoding
#pragma once
#include <strutil/readutil.h>
#include <helper/pushbacker.h>
#include <strutil/append.h>
#include <strutil/strutil.h>
#include <number/char_range.h>
#include <wrap/light/string.h>
#include <unicode/utf/view.h>
#include <limits>
#include <view/slice.h>
#include <number/array.h>
#include <strutil/equal.h>

namespace futils {

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

            template <class Header>
            constexpr bool encode_int(Header& result, const size_t bias, const size_t delta) {
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

            template <class C>
            constexpr void memmove(C* dst, C* src, size_t len) {
                if (dst < src) {
                    for (size_t i = 0; i < len; i++) {
                        dst[i] = src[i];
                    }
                }
                else {
                    for (size_t i = len; i > 0; i--) {
                        dst[i - 1] = src[i - 1];
                    }
                }
            }
        }  // namespace internal

        template <class String = wrap::string, class Out, class T>
        constexpr number::NumErr encode(Sequencer<T>& seq, Out& result) {
            static_assert(sizeof(typename Sequencer<T>::char_type) == 4, "need UTF-32 string");
            if (seq.eos()) {
                return number::NumError::invalid;
            }
            auto inipos = seq.rptr;
            size_t b, h;
            size_t delta, bias;
            size_t m, n;
            size_t si = 0, di = 0;
            String tmp{};
            if (!strutil::append_if(tmp, helper::nop, seq, [&](auto&& v) {
                    if (number::is_in_ascii_range(v)) {
                        di++;
                        return true;
                    }
                    return false;
                })) {
                if (di) {
                    strutil::append(result, tmp);
                    result.push_back('-');
                }
            }
            else {
                strutil::append(result, tmp);
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

        template <class String = wrap::string, class In, class Out>
        constexpr number::NumErr encode(In&& in, Out& result) {
            utf::U32View<buffer_t<In&>> view(in);
            auto seq = make_ref_seq(view);
            return encode<String>(seq, result);
        }

        template <class U32String = wrap::u32string, class Out, class T>
        constexpr number::NumErr decode(Sequencer<T>& seq, Out& result) {
            if (seq.eos()) {
                return true;
            }
            auto inipos = seq.rptr;
            std::uint32_t n = initial_n;
            size_t bias = initial_bias;
            size_t i = 0, out = 0;
            size_t b = 0, j = 0;
            U32String tmp{};
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
                internal::memmove(tmp.data() + i + 1, tmp.data() + i, (out - i) * sizeof(std::uint32_t));
                tmp[i] = n;
                i++;
                out++;
            }
            tmp.resize(out);
            utf::convert(tmp, result);
            return true;
        }

        template <class U32String = wrap::u32string, class In, class Out>
        constexpr number::NumErr decode(In&& in, Out& result) {
            auto seq = make_ref_seq(in);
            return decode<U32String>(seq, result);
        }

        template <class String = wrap::string, class In, class Out>
        constexpr number::NumErr encode_host(In&& in, Out& result) {
            auto seq = make_ref_seq(in);
            while (true) {
                auto inipos = seq.rptr;
                bool is_ascii = true;
                strutil::read_whilef<true>(helper::nop, seq, [&](auto&& c) {
                    if (!number::is_in_ascii_range(c)) {
                        is_ascii = false;
                    }
                    return c != '.';
                });
                if (is_ascii) {
                    auto end = seq.rptr;
                    seq.rptr = inipos;
                    if (!strutil::read_n(result, seq, end - inipos)) {
                        return number::NumError::invalid;
                    }
                }
                else {
                    strutil::append(result, "xn--");
                    auto slice = view::make_ref_slice(in, inipos, seq.rptr);
                    auto e = encode<String>(slice, result);
                    if (!e) {
                        return e;
                    }
                }
                if (seq.current() == '.') {
                    result.push_back('.');
                    seq.consume();
                    continue;
                }
                break;
            }
            return true;
        }

        template <class U32String = wrap::u32string, class In, class Out>
        constexpr number::NumErr decode_host(In&& in, Out& result) {
            auto seq = make_ref_seq(in);
            while (true) {
                bool is_ascii = true;
                bool is_international = seq.seek_if("xn--");
                auto inipos = seq.rptr;
                strutil::read_whilef<true>(helper::nop, seq, [&](auto&& c) {
                    if (!number::is_in_ascii_range(c)) {
                        is_ascii = false;
                        return false;
                    }
                    return c != '.';
                });
                if (!is_ascii) {
                    return number::NumError::invalid;
                }
                if (is_international) {
                    auto slice = view::make_ref_slice(in, inipos, seq.rptr);
                    auto e = decode<U32String>(slice, result);
                    if (!e) {
                        return e;
                    }
                }
                else {
                    auto end = seq.rptr;
                    seq.seek(inipos);
                    if (!strutil::read_n(result, seq, end - inipos)) {
                        return number::NumError::invalid;
                    }
                }
                if (seq.current() == '.') {
                    result.push_back('.');
                    seq.consume();
                    continue;
                }
                break;
            }
            return true;
        }

        namespace test {
            constexpr bool test_punycode() {
                auto test_encode_decode = [](auto input, auto expected_encoded) {
                    futils::helper::FixedPushBacker<char[50], 49> buf;
                    futils::helper::FixedPushBacker<char8_t[50], 49> buf2;
                    futils::punycode::encode<number::Array<char, 100>>(input, buf);
                    if (!futils::strutil::equal(buf.buf, expected_encoded)) {
                        return false;
                    }
                    futils::punycode::decode<number::Array<char32_t, 100>>(buf.buf, buf2);
                    if (!futils::strutil::equal(buf2.buf, input)) {
                        return false;
                    }
                    return true;
                };

                auto test_encode_decode_host = [](auto input, auto expected_encoded) {
                    futils::helper::FixedPushBacker<char[50], 49> buf;
                    futils::helper::FixedPushBacker<char8_t[50], 49> buf2;
                    futils::punycode::encode_host<number::Array<char, 100>>(input, buf);
                    if (!futils::strutil::equal(buf.buf, expected_encoded, futils::strutil::ignore_case())) {
                        return false;
                    }
                    futils::punycode::decode_host<number::Array<char32_t, 100>>(buf.buf, buf2);
                    if (!futils::strutil::equal(buf2.buf, input, futils::strutil::ignore_case())) {
                        return false;
                    }
                    return true;
                };

                return test_encode_decode(u8"ウィキペディア", "cckbak0byl6e") &&
                       test_encode_decode(u8"bücher", "bcher-kva") &&
                       test_encode_decode_host(u8"Go言語.com", u8"xn--go-hh0g6u.com") &&
                       test_encode_decode_host(u8"太郎.com", "xn--sssu80k.com") &&
                       test_encode_decode_host(u8"ドメイン名例.jp", "xn--eckwd4c7cu47r2wf.jp");
            }

            static_assert(test_punycode(), "punycode test failed");
        }  // namespace test

    }  // namespace punycode
}  // namespace futils

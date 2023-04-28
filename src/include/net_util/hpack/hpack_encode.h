/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hpack_huffman - hpack huffman codec
#pragma once
#include "../../wrap/light/enum.h"
#include "../../helper/equal.h"
#include "../../io/number.h"
#include "../../io/expandable_writer.h"
#include "hpack_huffman_table.h"
#include "../../io/bit.h"

namespace utils {
    namespace hpack {
        enum class HpackError {
            none,
            too_large_number,
            too_short_number,
            internal,
            input_length,
            invalid_mask,
            invalid_value,
            not_exists,
            table_update,
        };

        using HpkErr = wrap::EnumWrap<HpackError, HpackError::none, HpackError::internal>;

        template <class String>
        constexpr size_t gethuffmanlen(const String& str) {
            size_t ret = 0;
            for (auto& c : str) {
                ret += huffman::codes.codes[byte(c)].bits;
            }
            return (ret + 7) / 8;
        }

        template <class T, class String, class C, class U>
        constexpr void encode_huffman(io::basic_bit_writer<T, C, U>& vec, const String& in) {
            for (auto c : in) {
                huffman::codes.codes[byte(c)].write(vec);
            }
            while (!vec.is_aligned()) {
                vec.push_back(true);
            }
        }

        template <class T, class C, class U>
        constexpr HpkErr decode_huffman_achar(io::basic_bit_reader<T, C, U>& r,
                                              const file::gzip::huffman::DecodeTree*& fin,
                                              std::uint32_t& allone) {
            auto& table = huffman::decode_tree.table();
            auto t = table.get_root();
            bool f = false;
            for (;;) {
                if (!t) {
                    return HpackError::invalid_value;
                }
                if (t->has_value()) {
                    fin = t;
                    return HpackError::none;
                }
                if (!r.read(f)) {
                    return HpackError::too_short_number;
                }
                allone = (allone && f) ? allone + 1 : 0;
                t = table.get_next(t, f);
            }
        }

        template <class T, class Out, class C, class U>
        constexpr HpkErr decode_huffman(Out& res, io::basic_bit_reader<T, C, U>& r) {
            while (r.get_base().check_input(1)) {
                const file::gzip::huffman::DecodeTree* fin = nullptr;
                std::uint32_t allone = 1;
                auto tmp = decode_huffman_achar(r, fin, allone);
                if (tmp != HpackError::none) {
                    if (tmp == HpackError::too_short_number) {
                        if (r.get_base().check_input(1) && allone - 1 > 7) {
                            return HpackError::too_large_number;
                        }
                        break;
                    }
                    return tmp;
                }
                if (fin->get_value() >= 256) {
                    return HpackError::invalid_value;
                }
                res.push_back(byte(fin->get_value()));
            }
            return HpackError::none;
        }

        template <std::uint32_t n, class C, class U>
        constexpr HpkErr decode_integer(io::basic_reader<C, U>& r, size_t& value, std::uint8_t* firstmask = nullptr) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr byte msk = 0xff >> (8 - n);
            C tmp = 0;
            if (!r.read(view::basic_wvec<C, U>(&tmp, 1))) {
                return HpackError::input_length;
            }
            if (firstmask) {
                *firstmask = byte(tmp) & ~msk;
            }
            tmp &= msk;
            value = tmp;
            if (tmp < msk) {
                return true;
            }
            size_t m = 0;
            constexpr auto pow = [](size_t x) -> size_t {
                return (size_t(0x1) << 63) >> ((sizeof(size_t) * 8 - 1) - x);
            };
            do {
                if (!r.read(view::basic_wvec<C, U>(&tmp, 1))) {
                    return HpackError::input_length;
                }
                value += (byte(tmp) & 0x7f) * pow(m);
                m += 7;
                if (m > (sizeof(size_t) * 8 - 1)) {
                    return HpackError::too_large_number;
                }
            } while (tmp & 0x80);
            return true;
        }

        template <size_t n, class T, class C, class U>
        constexpr HpkErr encode_integer(io::basic_expand_writer<T, C, U>& w, size_t value, std::uint8_t firstmask) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr std::uint8_t msk = static_cast<std::uint8_t>(~0) >> (8 - n);
            if (firstmask & msk) {
                return HpackError::invalid_mask;
            }
            if (value < static_cast<size_t>(msk)) {
                w.write(byte(firstmask | value), 1);
            }
            else {
                w.write(byte(firstmask | msk), 1);
                value -= msk;
                while (value > 0x7f) {
                    w.write(byte((value % 0x80) | 0x80), 1);
                    value /= 0x80;
                }
                w.write(byte(value), 1);
            }
            return true;
        }

        template <size_t prefixed = 7, class T, class String, class C, class U>
        constexpr void encode_string(io::basic_expand_writer<T, C, U>& w, const String& value, C common_prefix = 0, C flag_huffman = 0x80) {
            if (auto huflen = gethuffmanlen(value); value.size() > huflen) {
                encode_integer<prefixed>(w, huflen, common_prefix | flag_huffman);
                w.expand(huflen);
                io::basic_bit_writer<view::basic_wvec<C, U>, C, U> vec{w.remain()};
                encode_huffman(vec, value);
            }
            else {
                encode_integer<prefixed>(w, value.size(), common_prefix);
                w.write(value);
            }
        }

        template <size_t prefixed = 7, class String, class C, class U>
        constexpr HpkErr decode_string(String& str, io::basic_reader<C, U>& r, C flag_huffman = 0x80, C* first_mask = nullptr) {
            size_t size = 0;
            byte mask = 0;
            if (!first_mask) {
                first_mask = &mask;
            }
            auto err = decode_integer<prefixed>(r, size, first_mask);
            if (err != HpackError::none) {
                return err;
            }
            auto [d, ok] = r.read(size);
            if (!ok) {
                return HpackError::input_length;
            }
            if (*first_mask & flag_huffman) {
                io::basic_bit_reader<view::basic_rvec<C, U>, C, U> r(d);
                return decode_huffman(str, r);
            }
            else {
                for (auto c : d) {
                    str.push_back(c);
                }
            }
            return true;
        }

    }  // namespace hpack
}  // namespace utils

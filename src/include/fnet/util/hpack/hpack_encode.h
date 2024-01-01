/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hpack_huffman - hpack huffman codec
#pragma once
#include <wrap/light/enum.h>
#include <strutil/equal.h>
#include <binary/number.h>
#include <binary/expandable_writer.h>
#include "hpack_huffman_table.h"
#include <binary/bit.h>

namespace futils {
    namespace hpack {
        enum class HpackError {
            none,
            too_large_number,
            too_short_number,
            internal,
            input_length,
            output_length,
            invalid_mask,
            invalid_value,
            not_exists,
            table_update,
        };

        using HpkErr = wrap::EnumWrap<HpackError, HpackError::none, HpackError::internal>;

        template <class String>
        constexpr size_t gethuffmanlen(const String& str) {
            size_t ret = 0;
            size_t size = 0;
            if constexpr (std::is_pointer_v<std::decay_t<decltype(str)>>) {
                size = futils::strlen(str);
            }
            else {
                size = str.size();
            }
            for (size_t i = 0; i < size; i++) {
                ret += huffman::codes.codes[byte(str[i])].bits;
            }
            return (ret + 7) / 8;
        }

        template <class T, class String, class C>
        constexpr void encode_huffman(binary::basic_bit_writer<T, C>& vec, const String& in) {
            size_t size = 0;
            if constexpr (std::is_pointer_v<std::decay_t<decltype(in)>>) {
                size = futils::strlen(in);
            }
            else {
                size = in.size();
            }
            for (size_t i = 0; i < size; i++) {
                huffman::codes.codes[byte(in[i])].write(vec);
            }
            while (!vec.is_aligned()) {
                vec.push_back(true);
            }
        }

        template <class T, class C>
        constexpr HpkErr decode_huffman_achar(binary::basic_bit_reader<T, C>& r,
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

        template <class T, class Out, class C>
        constexpr HpkErr decode_huffman(Out& res, binary::basic_bit_reader<T, C>& r) {
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

        template <std::uint32_t n, class C>
        constexpr HpkErr decode_integer(binary::basic_reader<C>& r, std::uint64_t& value, std::uint8_t* firstmask = nullptr) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr byte msk = 0xff >> (8 - n);
            C tmp = 0;
            if (!r.read(view::basic_wvec<C>(&tmp, 1))) {
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
            std::uint64_t m = 0;
            constexpr auto pow = [](std::uint64_t x) -> std::uint64_t {
                return (std::uint64_t(0x1) << 63) >> ((sizeof(std::uint64_t) * 8 - 1) - x);
            };
            do {
                if (!r.read(view::basic_wvec<C>(&tmp, 1))) {
                    return HpackError::input_length;
                }
                value += (byte(tmp) & 0x7f) * pow(m);
                m += 7;
                if (m > (sizeof(std::uint64_t) * 8 - 1)) {
                    return HpackError::too_large_number;
                }
            } while (tmp & 0x80);
            return true;
        }

        template <size_t n, class T, class C>
        constexpr HpkErr encode_integer(binary::basic_expand_writer<T, C>& w, size_t value, std::uint8_t firstmask) {
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

        template <size_t prefixed = 7, class T, class String, class C, class F1 = C, class F2 = C>
        constexpr HpkErr encode_string(binary::basic_expand_writer<T, C>& w, const String& value, F1 common_prefix = 0, F2 flag_huffman = 0x80) {
            size_t size = 0;
            if constexpr (std::is_pointer_v<std::decay_t<decltype(value)>>) {
                size = futils::strlen(value);
            }
            else {
                size = value.size();
            }
            if (auto huflen = gethuffmanlen(value); size > huflen) {
                auto err = encode_integer<prefixed>(w, huflen, common_prefix | flag_huffman);
                if (err != HpackError::none) {
                    return err;
                }
                w.expand(huflen);
                if (w.remain().size() != huflen) {
                    return HpackError::output_length;
                }
                binary::basic_bit_writer<view::basic_wvec<C>, C> vec{w.remain()};
                encode_huffman(vec, value);
                w.offset(huflen);
                return HpackError::none;
            }
            else {
                auto err = encode_integer<prefixed>(w, size, common_prefix);
                if (err != HpackError::none) {
                    return err;
                }
                if (!w.write(value)) {
                    return HpackError::output_length;
                }
                return HpackError::none;
            }
        }

        template <size_t prefixed = 7, class String, class C, class F1 = C>
        constexpr HpkErr decode_string(String& str, binary::basic_reader<C>& r, F1 flag_huffman = 0x80, C* first_mask = nullptr) {
            std::uint64_t size = 0;
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
                binary::basic_bit_reader<view::basic_rvec<C>, C> r(d);
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
}  // namespace futils

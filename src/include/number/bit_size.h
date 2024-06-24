/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/sequencer.h>
#include "parse.h"
#include <binary/log2i.h>
#include <binary/op.h>

namespace futils {
    namespace number {
        template <class T, class Vec, class Config = internal::ReadConfig>
        constexpr NumErr bit_size(size_t& res, Vec& buffer, Sequencer<T>& seq, int radix = 10, Config&& config = {}) {
            struct Prefix {
                Sequencer<T>& seq;
                size_t first_num = 0;
                size_t start = 0;
                bool first = true;
                size_t count = 0;

                constexpr void push_back(decltype(seq.current()) c) {
                    if (first) {
                        if (c == '0') {
                            return;
                        }
                        first_num = number_transform[c];
                        first = false;
                        start = seq.rptr;
                    }
                    count++;
                }
            } pb{seq};
            if (auto res = read_number(pb, seq, radix, nullptr, config); !res) {
                return res;
            }
            const auto is_pow2 = (radix & (radix - 1)) == 0;
            const auto log2_radix = binary::log2i(radix);
            if (is_pow2) {
                res = binary::log2i(pb.first_num) + (pb.count - 1) * log2_radix;
            }
            else {
                for (size_t i = pb.start; i < pb.start + pb.count; i++) {
                    binary::mul(buffer, radix);
                    binary::add(buffer, number::number_transform[seq.buf.buffer[i]]);
                }
                res = (buffer.size() - 1) * sizeof(buffer[0]) * 8 + binary::log2i(buffer[buffer.size() - 1]);
            }
            return NumError::none;
        }

        namespace test {
            constexpr std::pair<size_t, number::Array<std::uint64_t, 64>> large_decimal(const char* text) {
                auto seq = make_ref_seq(text);
                size_t res = 0;
                number::Array<uint64_t, 64> buffer{};
                auto err = bit_size(res, buffer, seq, 10);
                if (err != NumError::none) {
                    throw "error";
                }
                return {res, buffer};
            }
            constexpr auto test_large_decimal = large_decimal("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
            constexpr auto test_normal_decimal = large_decimal("1234567890");
            constexpr auto test_middle_decimal = large_decimal("12345678901234567890");
            static_assert(test_large_decimal.first == 650);
        }  // namespace test
    }  // namespace number
}  // namespace futils

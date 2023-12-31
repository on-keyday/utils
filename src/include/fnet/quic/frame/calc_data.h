/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// calc_data - calculate data size
#pragma once
#include <cstdint>
#include "../varint.h"

namespace utils {
    namespace fnet::quic::frame {
        struct StreamDataRange {
            std::uint64_t fixed_length = 0;
            std::uint64_t max_length = 0;
            bool min_zero = false;
            constexpr static std::uint64_t min_length_len = 1;

            constexpr std::uint64_t max_length_len() const noexcept {
                return varint::len(max_length);
            }

            constexpr std::uint64_t min_length() const noexcept {
                return min_zero || max_length == 0 ? 0 : 1;
            }

            // largest
            constexpr std::uint64_t length_exists_max_len() const noexcept {
                return fixed_length + max_length_len() + max_length;
            }

            // second largest
            constexpr std::uint64_t length_no_max_len() const noexcept {
                return fixed_length + max_length;
            }

            //  second smallest
            constexpr std::uint64_t length_exists_min_len() const noexcept {
                return fixed_length + min_length_len + min_length();
            }
            // smallest
            constexpr std::uint64_t length_no_min_len() const noexcept {
                return fixed_length + min_length();
            }
        };

        constexpr byte correct_delta(std::uint64_t to_fit, StreamDataRange range, std::int64_t data_len, byte delta) noexcept {
            for (auto i = 1; i <= delta; i++) {
                range.max_length = data_len + i;
                if (range.length_exists_max_len() > to_fit) {
                    return i - 1;  // add usage, able to use i-1 byte more for user data
                }
            }
            return delta;
        }

        constexpr std::tuple<std::uint64_t, std::uint64_t, bool> shrink_to_fit_with_len(std::uint64_t to_fit, StreamDataRange range) {
            if (!varint::in_range(range.max_length, 8)) {
                return {0, 0, false};
            }
            const std::uint64_t to_write = range.length_exists_max_len();
            std::int64_t data_len = range.max_length;
            auto length_len = varint::len(data_len);
            if (to_write > to_fit) {
                data_len -= to_write - to_fit;  // fix to fit
            }
            // data_len may have gap from actual usable data length because of difference between
            // variable integer encoding length of previous data len and of corrected data len
            // so from here, fix it
            byte fixed_length_len;
            // data_len can be minus now,
            // can be fixed by below
            if (data_len < 0) {
                fixed_length_len = 1;
            }
            else {
                fixed_length_len = varint::len(data_len);
            }
            const byte diff_varint_len = length_len - fixed_length_len;
            if (diff_varint_len != 0) {
                data_len += correct_delta(to_fit, range, data_len, diff_varint_len);
            }
            // oops cannot fix it
            if (range.min_zero ? data_len < 0 : data_len <= 0) {
                return {0, 0, false};
            }
            range.max_length = data_len;
            return {range.length_exists_max_len(), range.max_length, true};
        }

        // returns(frame_len,data_len,valid)
        constexpr std::tuple<std::uint64_t, std::uint64_t, bool> shrink_to_fit_no_len(std::uint64_t to_fit, StreamDataRange range) {
            // can write usage
            const std::uint64_t to_write = range.length_no_max_len();
            std::uint64_t& data_len = range.max_length;
            if (to_write > to_fit) {
                if (data_len < to_write - to_fit) {
                    return {0, 0, false};  // cannot send
                }
                data_len -= to_write - to_fit;
            }
            const size_t fixed_to_write = range.length_no_max_len();
            return {fixed_to_write, data_len, fixed_to_write == to_fit};
        }

    }  // namespace fnet::quic::frame
}  // namespace utils

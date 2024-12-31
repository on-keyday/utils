/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream - utf stream
#pragma once
#include "../binary/reader.h"
#include "convert.h"
#include "../../binary/view.h"
#include <tuple>

namespace futils::unicode::utf {
    enum class UTFType {
        unknown,
        utf8,
        utf16_be,
        utf16_le,
        utf32_be,
        utf32_le,
    };

    // returns (type,bom_len)
    template <class T>
    constexpr std::pair<UTFType, size_t> decide_type(Sequencer<T>& seq) {
        static_assert(sizeof(seq.current() == 1));
        byte data_[4]{
            byte(seq.current(0)),
            byte(seq.current(1)),
            byte(seq.current(2)),
            byte(seq.current(3)),
        };
        view::rvec data(data_, seq.remain() >= 4 ? 4 : seq.remain());
        if (data.size() >= 4) {
            if (data[0] == 0 &&
                data[1] == 0 &&
                ((data[2] == 0xFE &&
                  data[3] == 0xFF) ||
                 (data[2] == 0 &&
                  data[3] <= 0x7F))) {
                return {UTFType::utf32_be, data[2] == 0xFE ? 4 : 0};
            }
            else if (((data[0] == 0xFF &&
                       data[1] == 0xFE) ||
                      (data[0] <= 0x7f &&
                       data[1] == 0)) &&
                     data[2] == 0 &&
                     data[3] == 0) {
                return {UTFType::utf32_le, data[1] == 0xFE ? 4 : 0};
            }
        }
        if (data.size() >= 2) {
            if (data[0] == 0xFE &&
                data[1] == 0xFF) {
                return {UTFType::utf16_be, 2};
            }
            if (data[0] == 0 && data[1] <= 0x7F) {
                return {UTFType::utf16_be, 0};
            }
            if (data[0] == 0xFF && data[1] == 0xFE) {
                return {UTFType::utf16_le, 2};
            }
            if (data[0] <= 0x7F && data[1] == 0) {
                return {UTFType::utf16_le, 0};
            }
        }
        if (data.size() >= 3) {
            if (data[0] == 0xEF &&
                data[1] == 0xBB &&
                data[2] == 0xBF) {
                return {UTFType::utf8, 3};
            }
        }
        return {UTFType::utf8, 0};
    }

    template <class T>
    struct Stream {
        Sequencer<T> seq;
        static_assert(sizeof(seq.current() == 1), "need 1 byte sequencer value");
        UTFType type = UTFType::unknown;

        UTFErr read_impl(auto&& do_convert) {
            if (type == UTFType::unknown) {
                size_t skip;
                std::tie(type, skip) = decide_type(seq);
                seq.consume(skip);
            }
            if (seq.eos()) {
                return UTFError::utf_no_input;
            }
            byte data[4] = {byte(seq.current(0)), byte(seq.current(1)), byte(seq.current(2)), byte(seq.current(4))};
            auto u8view = view::rvec(data, seq.remain() >= 4 ? 4 : seq.remain());
            if (type == UTFType::utf8) {
                return do_convert(u8view);
            }
            if (type == UTFType::utf16_le || type == UTFType::utf16_be) {
                binary::EndianView<view::rvec, char16_t> u16view{u8view};
                u16view.little_endian = type == UTFType::utf16_le;
                return do_convert(u16view);
            }
            if (type == UTFType::utf32_le || type == UTFType::utf32_be) {
                binary::EndianView<view::rvec, char32_t> u32view{u8view};
                u32view.little_endian = type == UTFType::utf32_le;
                return do_convert(u32view);
            }
            return UTFError::utf_unknown;
        }

        std::pair<std::uint32_t, UTFErr> read_one(bool replace = false) {
            std::uint32_t code;
            auto do_convert = [&](auto&& view) {
                auto seq = make_ref_seq(view);
                return to_utf32(seq, code, false, replace);
            };
            auto err = read_impl(do_convert);
            return {code, err};
        }

        UTFErr read_one(auto& out, bool replace = false) {
            auto do_convert = [&](auto& view) -> UTFErr {
                auto seq = make_ref_seq(view);
                return utf::convert_one(seq, out, false, replace);
            };
            return read_impl(do_convert);
        }
    };
}  // namespace futils::unicode::utf

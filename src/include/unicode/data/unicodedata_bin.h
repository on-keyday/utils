/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/number.h>
#include "unicodedata.h"
#include <binary/term.h>

namespace futils {
    namespace unicode::data::bin {

        constexpr int enable_version = 5;

        bool write_string(auto& w, auto&& str, int version) {
            if (version < 5) {
                if (str.size() > 0xff) {
                    return false;
                }
                return w.write(byte(str.size()), 1) &&
                       w.write(view::rvec(str));
            }
            return binary::write_terminated(w, view::rvec(str));
        }

        inline bool read_string(auto& r, auto& str, int version) {
            view::rvec data;
            if (version < 5) {
                byte len = 0;
                if (!r.read(view::wvec(&len, 1))) {
                    return false;
                }
                if (!r.read(data, len)) {
                    return false;
                }
            }
            else {
                if (!binary::read_terminated(r, data)) {
                    return false;
                }
            }
            if constexpr (std::is_same_v<decltype(str), std::decay_t<decltype(data)>&>) {
                str = data;
            }
            else {
                str.assign(data.as_char<char>(), data.size());
            }
            return true;
        }

        template <class String>
        inline bool serialize_codeinfo(auto& w, const CodeInfo<String>& info, String& block, int version = enable_version) {
            if (version > enable_version) {
                return false;
            }
            if (!binary::write_num(w, info.codepoint) ||
                !write_string(w, info.name, version) ||
                !write_string(w, info.category, version) ||
                !binary::write_num(w, info.ccc) ||
                !write_string(w, info.bidiclass, version) ||
                !write_string(w, info.decomposition.command, version)) {
                return false;
            }
            // for compatibility use this value
            auto d = info.decomposition.to.size() * sizeof(char32_t);
            if (d > 0xff) {
                return false;
            }
            if (!binary::write_num(w, byte(d))) {
                return false;
            }
            for (auto c : info.decomposition.to) {
                if (!binary::write_num(w, c)) {
                    return false;
                }
            }
            auto write_numeric3 = [&] {
                if (info.numeric.flag & flags::large_num) {
                    return binary::write_num(w, info.numeric.v3_L);
                }
                else {
                    if (info.numeric.flag & flags::exist_one) {
                        if (!binary::write_num(w, info.numeric.v3_S._1)) {
                            return false;
                        }
                    }
                    if (info.numeric.flag & flags::exist_two) {
                        if (!binary::write_num(w, info.numeric.v3_S._2)) {
                            return false;
                        }
                    }
                }
                return true;
            };
            if (version <= 2) {
                if (!binary::write_num(w, byte(info.numeric.v1)) ||
                    !binary::write_num(w, byte(info.numeric.v2))) {
                    return false;
                }
                if (version == 0) {
                    std::string str = info.numeric.to_string();
                    if (!write_string(w, str, version)) {
                        return false;
                    }
                }
                else if (version >= 1) {
                    if (!binary::write_num(w, byte(info.numeric.flag))) {
                        return false;
                    }
                    if (!write_numeric3()) {
                        return false;
                    }
                }
            }
            else {
                byte fl = 0;
                if (version >= 4) {
                    if (info.block != block) {
                        fl = flags::has_block_name;
                    }
                }
                if (!binary::write_num(w, byte(info.numeric.flag | fl))) {
                    return false;
                }
                if (info.numeric.flag & flags::has_digit) {
                    if (!binary::write_num(w, byte(info.numeric.v1))) {
                        return false;
                    }
                }
                if (info.numeric.flag & flags::has_decimal) {
                    if (!binary::write_num(w, byte(info.numeric.v2))) {
                        return false;
                    }
                }
                if (!write_numeric3()) {
                    return false;
                }
            }
            if (!w.write(info.mirrored ? 1 : 0, 1)) {
                return false;
            }
            if (version <= 2) {
                if (!binary::write_num(w, info.casemap.upper) ||
                    !binary::write_num(w, info.casemap.lower) ||
                    !binary::write_num(w, info.casemap.title)) {
                    return false;
                }
            }
            else {
                if (!binary::write_num(w, byte(info.casemap.flag))) {
                    return false;
                }
                if (info.casemap.flag & flags::has_upper) {
                    if (!binary::write_num(w, info.casemap.upper)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_lower) {
                    if (!binary::write_num(w, info.casemap.lower)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_title) {
                    if (!binary::write_num(w, info.casemap.title)) {
                        return false;
                    }
                }
            }
            if (version >= 2) {
                if (!write_string(w, info.east_asian_width, version)) {
                    return false;
                }
            }
            if (version >= 4) {
                if (block != info.block) {
                    if (!write_string(w, info.block, version)) {
                        return false;
                    }
                }
            }
            if (version >= 5) {
                if (info.emoji_data.size() > 0xff) {
                    return false;
                }
                if (!w.write(byte(info.emoji_data.size()), 1)) {
                    return false;
                }
                for (auto& emoji_data : info.emoji_data) {
                    if (!write_string(w, emoji_data, version)) {
                        return false;
                    }
                }
            }
            return true;
        }

        template <class String>
        inline bool read_codeinfo(auto& r, CodeInfo<String>& info, String& block, int version = enable_version) {
            if (version > enable_version) {
                return false;
            }
            if (!binary::read_num(r, info.codepoint) ||
                !read_string(r, info.name, version) ||
                !read_string(r, info.category, version) ||
                !binary::read_num(r, info.ccc) ||
                !read_string(r, info.bidiclass, version) ||
                !read_string(r, info.decomposition.command, version)) {
                return false;
            }
            auto [size_, ok] = r.read(1);
            if (!ok) {
                return false;
            }
            auto size = size_[0] / sizeof(char32_t);
            for (auto i = 0; i < size; i++) {
                char32_t v;
                if (!binary::read_num(r, v)) {
                    return false;
                }
                info.decomposition.to.push_back(v);
            }
            auto read_numeric3 = [&] {
                if (info.numeric.flag & flags::large_num) {
                    if (!binary::read_num(r, info.numeric.v3_L)) {
                        return false;
                    }
                }
                else {
                    if (info.numeric.flag & flags::exist_one) {
                        if (!binary::read_num(r, info.numeric.v3_S._1)) {
                            return false;
                        }
                    }
                    if (info.numeric.flag & flags::exist_two) {
                        if (!binary::read_num(r, info.numeric.v3_S._2)) {
                            return false;
                        }
                    }
                }
                return true;
            };
            if (version <= 2) {
                byte v1, v2;
                if (!binary::read_num(r, v1) ||
                    !binary::read_num(r, v2)) {
                    return false;
                }
                info.numeric.v1 = v1;
                info.numeric.v2 = v2;
                if (version == 0) {
                    std::string str;
                    if (!read_string(r, str, version)) {
                        return false;
                    }
                    auto seq = make_ref_seq(str);
                    if (auto err = text::internal::parse_real(seq, info.numeric); err != text::ParseError::none) {
                        return false;
                    }
                }
                else if (version >= 1) {
                    if (!binary::read_num(r, info.numeric.flag)) {
                        return false;
                    }
                    if (!read_numeric3()) {
                        return false;
                    }
                }
            }
            else {
                byte v1, v2;
                if (!binary::read_num(r, info.numeric.flag)) {
                    return false;
                }
                if (info.numeric.flag & flags::has_digit) {
                    if (!binary::read_num(r, v1)) {
                        return false;
                    }
                    info.numeric.v1 = v1;
                }
                if (info.numeric.flag & flags::has_decimal) {
                    if (!binary::read_num(r, v2)) {
                        return false;
                    }
                    info.numeric.v2 = v2;
                }
                if (!read_numeric3()) {
                    return false;
                }
            }
            byte mil = 0;
            if (!r.read(view::wvec(&mil, 1))) {
                return false;
            }
            info.mirrored = mil ? true : false;
            if (version <= 2) {
                byte upper, lower, title;
                if (!binary::read_num(r, upper) ||
                    !binary::read_num(r, lower) ||
                    !binary::read_num(r, title)) {
                    return false;
                }
                info.casemap.upper = upper == 0xff ? invalid_char : upper;
                info.casemap.lower = lower == 0xff ? invalid_char : lower;
                info.casemap.title = title == 0xff ? invalid_char : title;
            }
            else {
                if (!binary::read_num(r, info.casemap.flag)) {
                    return false;
                }
                if (info.casemap.flag & flags::has_upper) {
                    if (!binary::read_num(r, info.casemap.upper)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_lower) {
                    if (!binary::read_num(r, info.casemap.lower)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_title) {
                    if (!binary::read_num(r, info.casemap.title)) {
                        return false;
                    }
                }
            }
            if (version >= 2) {
                if (!read_string(r, info.east_asian_width, version)) {
                    return false;
                }
            }
            else {
                data::internal::guess_east_asian_width(info);
            }
            if (version >= 4) {
                if (info.numeric.flag & flags::has_block_name) {
                    info.numeric.flag &= ~flags::has_block_name;
                    if (!read_string(r, block, version)) {
                        return false;
                    }
                }
                info.block = block;
            }
            if (version >= 5) {
                byte len = 0;
                if (!r.read(view::wvec(&len, 1))) {
                    return false;
                }
                for (auto i = 0; i < len; i++) {
                    String emoji;
                    if (!read_string(r, emoji, version)) {
                        return false;
                    }
                    info.emoji_data.push_back(std::move(emoji));
                }
            }
            return true;
        }

        template <class C, class String>
        inline bool serialize_unicodedata(binary::basic_writer<C>& w, const UnicodeData<String>& data, int version = enable_version) {
            auto write_version = [&] {
                return w.write('U', 1) &&
                       w.write('D', 1) &&
                       w.write('v', 1) &&
                       w.write('0' + version, 1);
            };
            switch (version) {
                default:
                    return false;
                case 0:
                    break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    write_version();
                    break;
            }
            String block;
            for (auto& d : data.codes) {
                if (!serialize_codeinfo(w, d.second, block, version)) {
                    return false;
                }
            }
            return true;
        }

        template <class C, class String>
        bool deserialize_unicodedata(binary::basic_reader<C>& r, UnicodeData<String>& data) {
            int version = 0;
            auto seq = make_ref_seq(r.remain());
            if (seq.seek_if("UDv1")) {
                version = 1;
            }
            else if (seq.seek_if("UDv2")) {
                version = 2;
            }
            else if (seq.seek_if("UDv3")) {
                version = 3;
            }
            else if (seq.seek_if("UDv4")) {
                version = 4;
            }
            else if (seq.seek_if("UDv5")) {
                version = 5;
            }
            else if (seq.seek_if("UDv")) {
                return false;
            }
            if (version != 0) {
                r.offset(4);
            }
            CodeInfo<String>* prev = nullptr;
            String block;
            while (!r.empty()) {
                CodeInfo<String> info;
                if (!read_codeinfo(r, info, block, version)) {
                    return false;
                }
                data::internal::set_codepoint_info(info, data, prev);
            }
            return true;
        }
    }  // namespace unicode::data::bin
}  // namespace futils

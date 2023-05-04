/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../io/number.h"
#include "../../io/ex_number.h"
#include "unicodedata.h"

namespace utils {
    namespace unicode::data::bin {

        constexpr int enable_version = 4;

        bool write_string(auto& w, auto&& str) {
            if (str.size() > 0xff) {
                return false;
            }
            return w.write(byte(str.size()), 1) &&
                   w.write(view::rvec(str));
        }

        inline bool read_string(auto& r, std::string& str) {
            byte len = 0;
            if (!r.read(view::wvec(&len, 1))) {
                return false;
            }
            auto [data, ok] = r.read(len);
            if (!ok) {
                return false;
            }
            str.assign(data.as_char(), data.size());
            return true;
        }

        inline bool serialize_codeinfo(auto& w, const CodeInfo& info, std::string& block, int version = enable_version) {
            if (version > enable_version) {
                return false;
            }
            if (!io::write_num(w, info.codepoint) ||
                !write_string(w, info.name) ||
                !write_string(w, info.category) ||
                !io::write_num(w, info.ccc) ||
                !write_string(w, info.bidiclass) ||
                !write_string(w, info.decomposition.command)) {
                return false;
            }
            // for compatibility use this value
            auto d = info.decomposition.to.size() * sizeof(char32_t);
            if (d > 0xff) {
                return false;
            }
            if (!io::write_num(w, byte(d))) {
                return false;
            }
            for (auto c : info.decomposition.to) {
                if (!io::write_num(w, c)) {
                    return false;
                }
            }
            auto write_numeric3 = [&] {
                if (info.numeric.flag & flags::large_num) {
                    return io::write_num(w, info.numeric.v3_L);
                }
                else {
                    if (info.numeric.flag & flags::exist_one) {
                        if (!io::write_num(w, info.numeric.v3_S._1)) {
                            return false;
                        }
                    }
                    if (info.numeric.flag & flags::exist_two) {
                        if (!io::write_num(w, info.numeric.v3_S._2)) {
                            return false;
                        }
                    }
                }
                return true;
            };
            if (version <= 2) {
                if (!io::write_num(w, byte(info.numeric.v1)) ||
                    !io::write_num(w, byte(info.numeric.v2))) {
                    return false;
                }
                if (version == 0) {
                    std::string str = info.numeric.to_string();
                    if (!write_string(w, str)) {
                        return false;
                    }
                }
                else if (version >= 1) {
                    if (!io::write_num(w, byte(info.numeric.flag))) {
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
                if (!io::write_num(w, byte(info.numeric.flag | fl))) {
                    return false;
                }
                if (info.numeric.flag & flags::has_digit) {
                    if (!io::write_num(w, byte(info.numeric.v1))) {
                        return false;
                    }
                }
                if (info.numeric.flag & flags::has_decimal) {
                    if (!io::write_num(w, byte(info.numeric.v2))) {
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
                if (!io::write_num(w, info.casemap.upper) ||
                    !io::write_num(w, info.casemap.lower) ||
                    !io::write_num(w, info.casemap.title)) {
                    return false;
                }
            }
            else {
                if (!io::write_num(w, byte(info.casemap.flag))) {
                    return false;
                }
                if (info.casemap.flag & flags::has_upper) {
                    if (!io::write_num(w, info.casemap.upper)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_lower) {
                    if (!io::write_num(w, info.casemap.lower)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_title) {
                    if (!io::write_num(w, info.casemap.title)) {
                        return false;
                    }
                }
            }
            if (version >= 2) {
                if (!write_string(w, info.east_asian_width)) {
                    return false;
                }
            }
            if (version >= 4) {
                if (block != info.block) {
                    if (!write_string(w, info.block)) {
                        return false;
                    }
                }
            }
            return true;
        }

        inline bool read_codeinfo(auto& r, CodeInfo& info, std::string& block, int version = enable_version) {
            if (version > enable_version) {
                return false;
            }
            if (!io::read_num(r, info.codepoint) ||
                !read_string(r, info.name) ||
                !read_string(r, info.category) ||
                !io::read_num(r, info.ccc) ||
                !read_string(r, info.bidiclass) ||
                !read_string(r, info.decomposition.command)) {
                return false;
            }
            auto [size_, ok] = r.read(1);
            if (!ok) {
                return false;
            }
            auto size = size_[0] / sizeof(char32_t);
            for (auto i = 0; i < size; i++) {
                char32_t v;
                if (!io::read_num(r, v)) {
                    return false;
                }
                info.decomposition.to.push_back(v);
            }
            auto read_numeric3 = [&] {
                if (info.numeric.flag & flags::large_num) {
                    if (!io::read_num(r, info.numeric.v3_L)) {
                        return false;
                    }
                }
                else {
                    if (info.numeric.flag & flags::exist_one) {
                        if (!io::read_num(r, info.numeric.v3_S._1)) {
                            return false;
                        }
                    }
                    if (info.numeric.flag & flags::exist_two) {
                        if (!io::read_num(r, info.numeric.v3_S._2)) {
                            return false;
                        }
                    }
                }
                return true;
            };
            if (version <= 2) {
                byte v1, v2;
                if (!io::read_num(r, v1) ||
                    !io::read_num(r, v2)) {
                    return false;
                }
                info.numeric.v1 = v1;
                info.numeric.v2 = v2;
                if (version == 0) {
                    std::string str;
                    if (!read_string(r, str)) {
                        return false;
                    }
                    auto seq = make_ref_seq(str);
                    if (auto err = text::internal::parse_real(seq, info.numeric); err != text::ParseError::none) {
                        return false;
                    }
                }
                else if (version >= 1) {
                    if (!io::read_num(r, info.numeric.flag)) {
                        return false;
                    }
                    if (!read_numeric3()) {
                        return false;
                    }
                }
            }
            else {
                byte v1, v2;
                if (!io::read_num(r, info.numeric.flag)) {
                    return false;
                }
                if (info.numeric.flag & flags::has_digit) {
                    if (!io::read_num(r, v1)) {
                        return false;
                    }
                    info.numeric.v1 = v1;
                }
                if (info.numeric.flag & flags::has_decimal) {
                    if (!io::read_num(r, v2)) {
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
                if (!io::read_num(r, upper) ||
                    !io::read_num(r, lower) ||
                    !io::read_num(r, title)) {
                    return false;
                }
                info.casemap.upper = upper == 0xff ? invalid_char : upper;
                info.casemap.lower = lower == 0xff ? invalid_char : lower;
                info.casemap.title = title == 0xff ? invalid_char : title;
            }
            else {
                if (!io::read_num(r, info.casemap.flag)) {
                    return false;
                }
                if (info.casemap.flag & flags::has_upper) {
                    if (!io::read_num(r, info.casemap.upper)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_lower) {
                    if (!io::read_num(r, info.casemap.lower)) {
                        return false;
                    }
                }
                if (info.casemap.flag & flags::has_title) {
                    if (!io::read_num(r, info.casemap.title)) {
                        return false;
                    }
                }
            }
            if (version >= 2) {
                if (!read_string(r, info.east_asian_width)) {
                    return false;
                }
            }
            else {
                data::internal::guess_east_asian_width(info);
            }
            if (version >= 4) {
                if (info.numeric.flag & flags::has_block_name) {
                    info.numeric.flag &= ~flags::has_block_name;
                    block.clear();
                    if (!read_string(r, block)) {
                        return false;
                    }
                }
                info.block = block;
            }
            return true;
        }

        template <class T, class C, class U>
        inline bool serialize_unicodedata(io::basic_expand_writer<T, C, U>& w, UnicodeData& data, int version = enable_version) {
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
                    write_version();
                    break;
            }
            std::string block = "";
            for (auto& d : data.codes) {
                if (!serialize_codeinfo(w, d.second, block, version)) {
                    return false;
                }
            }
            return true;
        }

        template <class C, class U>
        bool deserialize_unicodedata(io::basic_reader<C, U>& r, UnicodeData& data) {
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
            if (version != 0) {
                r.offset(4);
            }
            CodeInfo* prev = nullptr;
            std::string block;
            while (!r.empty()) {
                CodeInfo info;
                if (!read_codeinfo(r, info, block, version)) {
                    return false;
                }
                data::internal::set_codepoint_info(info, data, prev);
            }
            return true;
        }
    }  // namespace unicode::data::bin
}  // namespace utils

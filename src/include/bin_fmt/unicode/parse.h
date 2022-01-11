/*license*/

// parse - parse unicode data
#pragma once

#include "unicode_data.h"
#include "../../helper/strutil.h"
#include "../../utf/convert.h"

namespace utils {
    namespace unicode {
        template <class T>
        void parse_decomposition(T& s, Decomposition& res) {
            if (helper::equal(s, "")) return;
            auto c = helper::make_ref_splitview(s, " ");
            size_t pos = 0;
            if (c[0][0] == '<') {
                res.command = c[0];
                pos = 1;
            }
            for (auto i = pos; i < c.size(); i++) {
                unsigned int ch = 0;
                number::parse_integer(c[i], info.codepoint, 16);
                res.to.push_back((char32_t)ch);
            }
        }

        template <class T>
        void parse_real(T& str, Numeric& numeric) {
            if (str == "") return;
            auto seq = make_ref_seq(str);
            if (seq.seek_if("-")) {
                numeric.flag |= NumFlag::sign;
            }
            if (!seq.eos()) {
                size_t pos = seq.rptr;
                number::parse_integer(seq, numeric.v3_S._1);
                if (numeric.v3_S._1 == -1) {
                    r.seek(pos);
                    number::parse_integer(seq, numeric.v3_L);
                    numeric.flag |= NumFlag::large_num;
                }
                else {
                    numeric.flag |= NumFlag::exist_one;
                    if (r.seek_if("/")) {
                        number::parse_integer(seq, numeric.v3_S._2);
                        numeric.flag |= NumFlag::exist_two;
                    }
                }
            }
        }

        template <class T>
        void parse_numeric(T& d, CodeInfo& info) {
            if (d[6] != "") {
                //codepoint=(unsigned int)-1;
                number::parse_integer(info.numeric.v1, d[6]);
                info.numeric.flag |= NumFlag::has_digit;
                //info.numeric.v1=codepoint;
            }
            if (d[7] != "") {
                //codepoint=(unsigned int)-1;
                number::parse_integer(info.numeric.v2, d[7]);
                info.numeric.flag |= NumFlag::has_decimal;
                //info.numeric.v2=codepoint;
            }
            //info.numeric.v3=d[8];
            parse_real(d[8], info.numeric);
        }

        inline void guess_east_asian_wide(CodeInfo& info) {
            if (info.decomposition.command == "<wide>") {
                info.east_asian_width = "F";
            }
            else if (info.decomposition.command == "<narrow>") {
                info.east_asian_width = "H";
            }
            else if (info.name.find("CJK") != ~0 || info.name.find("HIRAGANA") != ~0 ||
                     info.name.find("KATAKANA") != ~0) {
                info.east_asian_width = "W";
            }
            else if (info.name.find("GREEK") != ~0) {
                info.east_asian_width = "A";
            }
            else {
                info.east_asian_width = "U";
            }
        }

        template <class T>
        bool parse_codepoint(T& d, CodeInfo& info) {
            if (d.size() < 14) return false;
            char32_t codepoint = (char32_t)-1;
            if (!number::parse_integer(d[0], info.codepoint, 16)) {
                return false;
            }
            utf::convert(d[1], info.name);
            utf::convert(d[2], info.category);
            if (!number::parse_integer(d[3], info.ccc)) {
                return false;
            }
            utf::convert(d[4], info.bidiclass);
            parse_decomposition(d[5], info.decomposition);
            parse_numeric(d, info);
            if (!helper::equal(d[9], "Y") && !helper::equal(d[9], "N")) return false;
            info.mirrored = !helper::equal(d[9], "Y") ? true : false;
            parse_case(d, info.casemap);
            guess_east_asian_wide(info);
            return true;
        }

        inline void set_codepoint_info(CodeInfo& info, UnicodeData& ret, CodeInfo*& prev) {
            auto& point = ret.codes[info.codepoint];
            ret.names.emplace(info.name, &point);
            ret.categorys.emplace(info.category, &point);
            if (prev) {
                if (info.codepoint != prev->codepoint + 1 && info.name.back() == '>' &&
                    prev->name.back() == '>' && !prev->range) {
                    ret.ranges.push_back(CodeRange{prev, &point});
                    info.range = prev;
                    prev->range = &point;
                }
            }
            if (info.decomposition.to.size()) {
                ret.composition.emplace(info.decomposition.to, &point);
            }
            point = std::move(info);
            prev = &point;
        }

        template <class T>
        bool parse_unicodedata(T& data, UnicodeData& ret) {
            CodeInfo* prev = nullptr;
            for (auto& d : data) {
                auto splt = helper::make_ref_splitview(d, ";");
                CodeInfo info;
                if (!parse_codepoint(d, info)) {
                    return false;
                }
                set_codepoint_info(info, ret, prev);
            }
            return true;
        }
    }  // namespace unicode
}  // namespace utils

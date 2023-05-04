/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <string>
#include "../../core/byte.h"
#include <cstdint>
#include "../../number/to_string.h"
#include <map>
#include <vector>

namespace utils {
    namespace unicode::data {
        struct Decomposition {
            std::string command;
            std::u32string to;
        };
        namespace flags {
            enum RecordFlags : byte {
                none = 0,
                exist_one = 0x1,
                exist_two = 0x2,
                sign = 0x4,
                large_num = 0x8,
                has_digit = 0x10,
                has_decimal = 0x20,
                has_block_name = 0x40,
            };

            enum CaseFlags : byte {
                no_case = 0,
                has_upper = 0x1,
                has_lower = 0x2,
                has_title = 0x4,
            };
        }  // namespace flags

        struct Numeric {
            std::int32_t v1 = -1;
            std::int32_t v2 = -1;
            union {
                struct {
                    std::int32_t _1;
                    std::int32_t _2;
                } v3_S;
                std::int64_t v3_L = -1;
            };
            byte flag = 0;

            std::string to_string() const {
                if (flag == 0) {
                    return "";
                }
                std::string ret;
                ret = (flag & flags::sign ? "-" : "");
                if (flag & flags::large_num) {
                    ret += number::to_string<std::string>(v3_L);
                }
                else {
                    ret += std::to_string(v3_S._1);
                    if (flag & flags::exist_two) {
                        ret += "/" + number::to_string<std::string>(v3_S._2);
                    }
                }
                return ret;
            }

            double num() const {
                double ret = 0;
                if (!flag) return NAN;
                if (flag & flags::large_num) {
                    ret = (double)v3_L;
                }
                else {
                    ret = (double)v3_S._1;
                    if (flag & flags::exist_two) {
                        ret = ret / v3_S._2;
                    }
                }
                if (flag & flags::sign) {
                    ret = -ret;
                }
                return ret;
            }
        };

        constexpr auto invalid_char = char32_t(-1);

        struct CaseMap {
            char32_t upper = invalid_char;
            char32_t lower = invalid_char;
            char32_t title = invalid_char;
            byte flag = 0;
        };

        struct CodeInfo {
            char32_t codepoint;
            std::string name;
            std::string block;
            std::string category;
            unsigned int ccc = 0;  // Canonical_Combining_Class
            std::string bidiclass;
            std::string east_asian_width;
            Decomposition decomposition;
            Numeric numeric;
            bool mirrored = false;
            CaseMap casemap;
            CodeInfo* range = nullptr;
        };

        struct CodeRange {
            CodeInfo* begin = nullptr;
            CodeInfo* end = nullptr;
        };

        struct UnicodeData {
            std::map<char32_t, CodeInfo> codes;
            std::multimap<std::string, CodeInfo*> names;
            std::multimap<std::string, CodeInfo*> categorys;
            std::vector<CodeRange> ranges;
            std::multimap<std::u32string, CodeInfo*> composition;
        };

        namespace internal {
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

            inline void guess_east_asian_width(CodeInfo& info) {
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

        }  // namespace internal
    }      // namespace unicode::data
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// unicode_data - unicode data
// almost copied from commonlib
#pragma once

#include "../../wrap/light/string.h"
#include "../../number/to_string.h"
#include "../../number/parse.h"
#include "../../wrap/light/enum.h"
#include "../../wrap/light/vector.h"
#include "../../wrap/light/map.h"

namespace utils {
    namespace unicode {
        struct Decomposition {
            wrap::string command;
            wrap::u32string to;
        };
        enum class NumFlag {
            none = 0,
            exist_one = 0x1,
            exist_two = 0x2,
            sign = 0x4,
            large_num = 0x8,
            has_digit = 0x10,
            has_decimal = 0x20,
            has_blockname = 0x40,
        };

        DEFINE_ENUM_FLAGOP(NumFlag)

        enum class CaseFlag {
            none = 0,
            has_upper = 0x1,
            has_lower = 0x2,
            has_title = 0x4,
        };

        DEFINE_ENUM_FLAGOP(CaseFlag)

        struct Numeric {
            int v1 = (int)-1;
            int v2 = (int)-1;
            // std::string v3;
            NumFlag flag = NumFlag::none;
            union {
                struct {
                    int _1;
                    int _2;
                } v3_S;
                long long v3_L = -1;
            };
            // std::string v3_str;

            template <class String>
            bool stringify(String& str) {
                if (!any(flag)) return false;
                if (any(flag & NumFlag::sign)) {
                    str.push_back('-');
                }
                if (any(flag & NumFlag::large_num)) {
                    number::to_string(str, v3_L);
                }
                else {
                    number::to_string(str, v3_S._1);
                    if (flag & NumFlag::exist_two) {
                        str.push_back('/');
                        number::to_string(str, v3_S._2);
                    }
                }
                return true;
            }

            double num() const {
                double ret = 0;
                if (!any(flag)) return std::numeric_limits<double>::quiet_NaN();
                if (any(flag & NumFlag::large_num)) {
                    ret = (double)v3_L;
                }
                else {
                    ret = (double)v3_S._1;
                    if (any(flag & NumFlag::exist_two)) {
                        ret = ret / v3_S._2;
                    }
                }
                if (any(flag & NumFlag::sign)) {
                    ret = -ret;
                }
                return ret;
            }
        };

        struct CaseMap {
            char32_t upper = (char32_t)-1;
            char32_t lower = (char32_t)-1;
            char32_t title = (char32_t)-1;
            CaseFlag flag = CaseFlag::none;
        };

        struct CodeInfo {
            char32_t codepoint;
            wrap::string name;
            wrap::string block;
            wrap::string category;
            unsigned int ccc = 0;  // Canonical_Combining_Class
            wrap::string bidiclass;
            wrap::string east_asian_width;
            wrap::string breakcategory;
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
            wrap::map<char32_t, CodeInfo> codes;
            wrap::multimap<wrap::string, CodeInfo*> names;
            wrap::multimap<wrap::string, CodeInfo*> categorys;
            wrap::vector<CodeRange> ranges;
            wrap::multimap<wrap::u32string, CodeInfo*> composition;
        };

    }  // namespace unicode
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../view/iovec.h"
#include "../../../number/to_string.h"
#include "../../../helper/appender.h"

namespace utils {
    namespace dnet::quic::log {
        auto fmt_numfield(auto& out) {
            return [&](const char* name, std::uint64_t num, int radix = 10) {
                helper::appends(out, " ", name, ": ");
                if (radix == 16) {
                    helper::append(out, "0x");
                }
                number::to_string(out, num, radix);
                helper::appends(out, ",");
            };
        }

        auto fmt_hexfield(auto& out) {
            return [&](const char* name, view::rvec data) {
                helper::appends(out, " ", name, ": 0x");
                for (auto c : data) {
                    if (c < 0x10) {
                        out.push_back('0');
                    }
                    number::to_string(out, c, 16);
                }
                helper::appends(out, ",");
            };
        }

        auto fmt_datafield(auto& out, bool omit_data, bool data_as_hex) {
            return [=, &out](const char* name, view::rvec data) {
                if (omit_data) {
                    return;
                }
                if (data_as_hex) {
                    fmt_hexfield(out)(name, data);
                    return;
                }
                helper::appends(out, " ", name, ": \"");
                for (auto c : data) {
                    if (0x21 <= c && c <= 0x7E) {
                        if (c == '\\' || c == '"') {
                            out.push_back('\\');
                        }
                        out.push_back(c);
                    }
                    else {
                        helper::append(out, "\\x");
                        if (c < 0x10) {
                            out.push_back('0');
                        }
                        number::to_string(out, c, 16);
                    }
                }
                helper::append(out, "\",");
            };
        }
        auto fmt_typefield(auto& out) {
            return [&](FrameFlags type) {
                const char* str = to_string(type.type_detail());
                if (str) {
                    helper::appends(out, str);
                }
                else {
                    helper::appends(out, "UNKNOWN(0x");
                    number::to_string(out, type.value, 16);
                    helper::append(out, ")");
                }
                if (type.STREAM_off()) {
                    helper::append(out, "+OFF");
                }
                if (type.STREAM_len()) {
                    helper::append(out, "+LEN");
                }
                if (type.STREAM_fin()) {
                    helper::append(out, "+FIN");
                }
            };
        }

        auto fmt_group(auto& out) {
            return [&](const char* name, bool is_array, auto&& cb) {
                if (name) {
                    helper::appends(out, " ", name, ":");
                }
                helper::append(out, is_array ? " [" : " {");
                cb();
                helper::append(out, is_array ? "]," : "},");
            };
        }
    }  // namespace dnet::quic::log
}  // namespace utils

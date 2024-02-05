/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../view/iovec.h"
#include "../../../number/to_string.h"
#include "../../../strutil/append.h"
#include "../../../escape/escape.h"

namespace futils {
    namespace fnet::quic::log {

        auto fmt_numfield(auto& out) {
            return [&](const char* name, std::uint64_t num, int radix = 10, bool last_comma = true) {
                strutil::appends(out, " ", name, ": ");
                if (radix == 16) {
                    strutil::append(out, "0x");
                }
                number::to_string(out, num, radix);
                strutil::appends(out, last_comma ? "," : " ");
            };
        }

        auto fmt_hexfield(auto& out) {
            return [&](const char* name, view::rvec data, bool last_comma = true) {
                if (name) {
                    strutil::appends(out, " ", name, ":");
                }
                if (data.size() == 0) {
                    strutil::append(out, "(empty)");
                    return;
                }
                strutil::append(out, " 0x");
                for (auto c : data) {
                    if (c < 0x10) {
                        out.push_back('0');
                    }
                    number::to_string(out, c, 16);
                }
                strutil::appends(out, last_comma ? "," : " ");
            };
        }

        auto fmt_datafield(auto& out, bool omit_data, bool data_as_hex) {
            return [=, &out](const char* name, view::rvec data, bool last_comma = true) {
                if (omit_data) {
                    return;
                }
                if (data_as_hex) {
                    fmt_hexfield(out)(name, data);
                    return;
                }
                if (name) {
                    strutil::appends(out, " ", name, ":");
                }
                strutil::append(out, " \"");
                for (auto c : data) {
                    if (0x21 <= c && c <= 0x7E) {
                        if (c == '\\' || c == '"') {
                            out.push_back('\\');
                        }
                        out.push_back(c);
                    }
                    else {
                        strutil::append(out, "\\x");
                        if (c < 0x10) {
                            out.push_back('0');
                        }
                        number::to_string(out, c, 16);
                    }
                }
                strutil::appends(out, "\"", last_comma ? "," : " ");
            };
        }

        auto fmt_typefield() {
            return [&](auto& out, FrameFlags type) {
                const char* str = to_string(type.type_detail());
                if (str) {
                    strutil::appends(out, str);
                }
                else {
                    strutil::appends(out, "UNKNOWN(0x");
                    number::to_string(out, type.value, 16);
                    strutil::append(out, ")");
                }
                if (type.STREAM_off()) {
                    strutil::append(out, "+OFF");
                }
                if (type.STREAM_len()) {
                    strutil::append(out, "+LEN");
                }
                if (type.STREAM_fin()) {
                    strutil::append(out, "+FIN");
                }
            };
        }
        auto fmt_group(auto& out) {
            return [&](const char* name, bool is_array, bool last_comma, auto&& cb) {
                if (name) {
                    strutil::appends(out, " ", name, ":");
                }
                strutil::append(out, is_array ? " [" : " {");
                cb();
                strutil::appends(out, is_array ? "]" : "}", last_comma ? "," : " ");
            };
        }

        auto fmt_key_value(auto& out) {
            return [&](auto&& key_cb, auto&& value_cb, bool last_comma = true) {
                strutil::append(out, " ");
                key_cb();
                strutil::append(out, ": ");
                value_cb();
                strutil::append(out, last_comma ? "," : " ");
            };
        }

        auto fmt_type() {
            return [](auto& out, auto type) {
                const char* str = to_string(type);
                if (str) {
                    strutil::appends(out, str);
                }
                else {
                    strutil::appends(out, "UNKNOWN(0x");
                    number::to_string(out, std::uint64_t(type), 16);
                    strutil::append(out, ")");
                }
            };
        }

        auto fmt_frame_type() {
            auto typ = fmt_type();
            return [=](auto& out, FrameFlags type) {
                typ(out, type.type_detail());
                if (type.STREAM_off()) {
                    strutil::append(out, "+OFF");
                }
                if (type.STREAM_len()) {
                    strutil::append(out, "+LEN");
                }
                if (type.STREAM_fin()) {
                    strutil::append(out, "+FIN");
                }
            };
        }

        constexpr auto fmt_string() {
            return [](auto& out, view::rvec data) {
                strutil::append(out, "\"");
                escape::escape_str(data, out, escape::EscapeFlag::hex);
                strutil::append(out, "\"");
            };
        }

        constexpr auto fmt_int() {
            return [](auto& out, std::uint64_t num, int radix = 10) {
                if (radix == 16) {
                    strutil::append(out, "0x");
                }
                number::to_string(out, num, radix);
            };
        }

        constexpr auto fmt_hex() {
            return [](auto& out, view::rvec data) {
                if (data.size() == 0) {
                    strutil::append(out, "(empty)");
                    return;
                }
                strutil::append(out, " 0x");
                for (auto c : data) {
                    if (c < 0x10) {
                        out.push_back('0');
                    }
                    number::to_string(out, c, 16);
                }
            };
        }

        auto fmt_field(auto& out, auto format, bool name_as_string = false) {
            return [=, &out](const char* name, auto&&... data) {
                if (name) {
                    if (name_as_string) {
                        strutil::append(out, "\"");
                        escape::escape_str(name, out, escape::EscapeFlag::hex);
                        strutil::append(out, "\": ");
                    }
                    else {
                        strutil::appends(out, name, ": ");
                    }
                }
                format(out, data...);
                strutil::append(out, ", ");
            };
        }

        template <class T>
        concept has_pop_back_and_back = requires(T t) {
                                            { t.back() };
                                            { t.pop_back() };
                                            { t.size() };
                                        };

        void erase_comma_if_exist(auto& out, bool no_space = false) {
            if constexpr (has_pop_back_and_back<decltype(out)>) {
                while (out.size() && (out.back() == ' ' || out.back() == ',')) {
                    out.pop_back();
                }
                if (no_space || (out.size() && (out.back() == '[' || out.back() == '{'))) {
                    return;
                }
                strutil::append(out, " ");
            }
        }

        constexpr auto fmt_object_or_array(auto& out, bool name_as_string = false) {
            return [=, &out](const char* name, bool is_array, auto&& fields) {
                if (name) {
                    if (name_as_string) {
                        strutil::append(out, "\"");
                        escape::escape_str(name, out, escape::EscapeFlag::hex);
                        strutil::append(out, "\": ");
                    }
                    else {
                        strutil::appends(out, name, ": ");
                    }
                }
                strutil::append(out, is_array ? "[ " : "{ ");
                fields();
                erase_comma_if_exist(out);
                strutil::append(out, is_array ? "], " : "}, ");
            };
        }

    }  // namespace fnet::quic::log
}  // namespace futils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// escape - string escape sequence
#pragma once

#include "../core/sequencer.h"
#include "../helper/appender.h"
#include "../utf/convert.h"
#include "../number/char_range.h"
#include "../number/to_string.h"
#include "../number/parse.h"

namespace utils {
    namespace escape {

        enum class EscapeFlag {
            none = 0,
            utf = 0x1,
            oct = 0x2,
            hex = 0x4,
        };

        DEFINE_ENUM_FLAGOP(EscapeFlag)

        template <class In, class Out>
        constexpr bool escape_str(Sequencer<In>& seq, Out& out, EscapeFlag flag = EscapeFlag::none) {
            while (!seq.eos()) {
                auto c = seq.current();
                if (c == '\n') {
                    helper::append(out, "\\n");
                }
                else if (c == '\r') {
                    helper::append(out, "\\r");
                }
                else if (c == '\t') {
                    helper::append(out, "\\t");
                }
                else if (c == '\a') {
                    helper::append(out, "\\a");
                }
                else if (c == '\b') {
                    helper::append(out, "\\b");
                }
                else if (c == '\v') {
                    helper::append(out, "\\v");
                }
                else if (c == '\e') {
                    helper::append(out, "\\e");
                }
                else if (c == '\f') {
                    helper::append(out, "\\f");
                }
                else if (c == '\'') {
                    helper::append(out, "\\'");
                }
                else if (c == '\\') {
                    helper::append(out, "\\\\");
                }
                else if (c == '\"') {
                    helper::append(out, "\\\"");
                }
                else if (c != ' ' && !number::is_in_visible_range(c)) {
                    bool done = false;
                    if (any(flag & EscapeFlag::utf)) {
                        auto s = seq.rptr;
                        utf::U32Buffer buf;
                        if (utf::convert_one(seq, buf)) {
                            auto set_one = [&](auto n) {
                                helper::append(out, "\\u");
                                if (n < 0x1000) {
                                    out.push_back('0');
                                }
                                if (n < 0x100) {
                                    out.push_back('0');
                                }
                                if (n < 0x10) {
                                    out.push_back('0');
                                }
                                if (!number::to_string(out, n, 16)) {
                                    return false;
                                }
                                return true;
                            };
                            for (auto i = 0; i < buf.size(); i++) {
                                if (!set_one(buf[i])) {
                                    return false;
                                }
                            }
                            done = true;
                            seq.backto();
                        }
                        else {
                            seq.rptr = s;
                        }
                    }
                    if (!done && any(flag & EscapeFlag::hex)) {
                        helper::append(out, "\\x");
                        if (!number::to_string(out, c, 16)) {
                            return false;
                        }
                        done = true;
                    }
                    if (!done && any(EscapeFlag::oct)) {
                        helper::append(out, "\\");
                        if (!number::to_string(out, c, 8)) {
                            return false;
                        }
                    }
                    if (!done) {
                        out.push_back(c);
                    }
                }
                else {
                    out.push_back(c);
                }
                seq.consume();
            }
            return true;
        }

        template <class In, class Out>
        constexpr bool unescape_str(Sequencer<In>& seq, Out& out, EscapeFlag flag = EscapeFlag::none) {
            while (!seq.eos()) {
                auto c = seq.current();
                if (c == '\\') {
                    seq.consume();
                    if (seq.eos()) {
                        return false;
                    }
                    c = seq.current();
                    if (c == 'n') {
                        out.push_back('\n');
                    }
                    else if (c == 'r') {
                        out.push_back('\r');
                    }
                    else if (c == 't') {
                        out.push_back('\t');
                    }
                    else if (c == 'a') {
                        out.push_back('\a');
                    }
                    else if (c == 'b') {
                        out.push_back('\b');
                    }
                    else if (c == 'v') {
                        out.push_back('\v');
                    }
                    else if (c == 'e') {
                        out.push_back('\e');
                    }
                    else if (c == 'f') {
                        out.push_back('\f');
                    }
                    else if (c == 'x') {
                        seq.consume();
                        if (seq.eos()) {
                            return false;
                        }
                    }
                }
                else {
                    out.push_back(c);
                }
                seq.consume();
            }
        }
    }  // namespace escape
}  // namespace utils

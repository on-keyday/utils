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
namespace utils {
    namespace escape {

        template <class In, class Out>
        constexpr bool escape_str(Sequencer<In>& seq, Out& out) {
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
                else {
                    out.push_back(c);
                }
            }
        }
    }  // namespace escape
}  // namespace utils
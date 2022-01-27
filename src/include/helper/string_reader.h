/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// string_reader - string reader
#pragma once

#include "../core/sequencer.h"
#include "appender.h"
#include "strutil.h"

namespace utils {
    namespace helper {
        constexpr auto string_reader(auto& end, auto& esc, bool allow_line) {
            return [=](auto& seq, auto& tok, int& flag) {
                while (!seq.eos()) {
                    if (auto n = seq.match_n(end)) {
                        auto sz = bufsize(esc);
                        if (sz) {
                            if (helper::ends_with(tok, esc)) {
                                auto sl = helper::make_ref_slice(tok, 0, token.size() - sz);
                                if (!helper::ends_with(sl, esc)) {
                                    flag = 1;
                                    return true;
                                }
                                seq.consume(n);
                                helper::append(tok, end);
                                continue;
                            }
                        }
                        flag = 1;
                        return true;
                    }
                    auto c = seq.current();
                    if (c == '\n' || c == '\r') {
                        if (!allow_line) {
                            flag = -1;
                            return false;
                        }
                    }
                    tok->token.push_back(c);
                    seq.consume();
                }
                flag = -1;
                return false;
            };
        }
    }  // namespace helper
}  // namespace utils

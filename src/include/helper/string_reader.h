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
        constexpr auto string_reader(auto&& end, auto&& esc, bool allow_line = false) {
            return [=](auto& seq, auto& tok) {
                while (!seq.eos()) {
                    if (auto n = seq.match_n(end)) {
                        auto sz = bufsize(esc);
                        if (sz) {
                            if (ends_with(tok, esc)) {
                                auto sl = make_ref_slice(tok, 0, tok.size() - sz);
                                if (!ends_with(sl, esc)) {
                                    return true;
                                }
                                seq.consume(n);
                                append(tok, end);
                                continue;
                            }
                        }
                        return true;
                    }
                    auto c = seq.current();
                    if (c == '\n' || c == '\r') {
                        if (!allow_line) {
                            return false;
                        }
                    }
                    tok.push_back(c);
                    seq.consume();
                }
                return false;
            };
        }
    }  // namespace helper
}  // namespace utils

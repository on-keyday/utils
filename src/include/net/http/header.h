/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header - parse http1 header
#pragma once

#include "../../helper/readutil.h"
#include "../../helper/pushbacker.h"

namespace utils {
    namespace net {
        template <class String, class T, class Result>
        bool header_parse_common(Sequencer<T>& seq, Result& result) {
            while (!seq.eos()) {
                if (helper::match_eol(seq)) {
                    break;
                }
                String key, value;
                if (!helper::read_while<true>(key, seq, [](auto v) {
                        return v != ':';
                    })) {
                    return false;
                }
                if (!seq.seek_if(":")) {
                    return false;
                }
                helper::read_while(helper::nop, seq, [](auto v) {
                    return v == ' ';
                });
                if (!helper::read_while<true>(value, seq, [](auto v) {
                        return v != '\r' && v != '\n';
                    })) {
                    return false;
                }
                result.emplace(std::move(key), std::move(value));
                if (!helper::match_eol(seq)) {
                    return false;
                }
            }
        }
    }  // namespace net
}  // namespace utils

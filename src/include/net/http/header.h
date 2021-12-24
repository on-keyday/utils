/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header - parse http1 header
#pragma once

#include "../../helper/readutil.h"
#include "../../helper/view.h"

namespace utils {
    namespace net {
        template <class String, class T, class Result>
        bool header_parse_common(Sequencer<T>& seq, Result& result) {
            while (!seq.eos()) {
                String key, value;
                helper::read_while(key, seq, [](auto v) {
                    return v != ':';
                });
                if (!seq.seek_if(":")) {
                    return false;
                }
                helper::read_while(helper::nop)
            }
        }
    }  // namespace net
}  // namespace utils

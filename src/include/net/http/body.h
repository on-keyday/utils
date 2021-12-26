/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// body - http body reading
#pragma once

#include "../../core/sequencer.h"
#include "../core/iodef.h"
#include "../../helper/readutil.h"
#include "../../number/parse.h"

namespace utils {
    namespace net {
        namespace h1body {
            enum class BodyType {
                no_info,
                chuncked,
                content_length,
            };

            template <class String, class T>
            State read_body(String& result, Sequencer<T>& seq, BodyType type = BodyType::no_info, size_t expect = 0) {
                auto inipos = seq.rptr;
                if (type == BodyType::chuncked) {
                    size_t num = 0;
                    auto e = number::parse_integer(seq, num, 16);
                    if (e != number::NumError::none) {
                        return State::failed;
                    }
                    if (!helper::match_eol(seq)) {
                        return State::failed;
                    }
                }
                else if (type == BodyType::content_length) {
                    if (seq.remain() < expect) {
                        return State::running;
                    }
                    helper::read_n(result, seq, expect);
                    return State::complete;
                }
                else {
                    helper::read_all(result, seq);
                    return State::complete;
                }
            }
        }  // namespace h1body

    }  // namespace net
}  // namespace utils

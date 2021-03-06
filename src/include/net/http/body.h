/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// body - http body reading
#pragma once

#include "../../core/sequencer.h"
#include "../core/iodef.h"
#include "../../helper/readutil.h"
#include "../../number/parse.h"
#include <helper/equal.h>

namespace utils {
    namespace net {
        namespace h1body {
            enum class BodyType {
                no_info,
                chuncked,
                content_length,
            };

            template <class String, class T>
            State read_body(String& result, Sequencer<T>& seq, size_t& expect, BodyType type = BodyType::no_info) {
                auto inipos = seq.rptr;
                if (type == BodyType::chuncked) {
                    while (true) {
                        helper::match_eol(seq);
                        if (seq.eos()) {
                            return State::running;
                        }
                        size_t num = 0;
                        auto e = number::parse_integer(seq, num, 16);
                        if (e != number::NumError::none) {
                            return State::failed;
                        }
                        if (!helper::match_eol(seq)) {
                            return State::failed;
                        }
                        if (num != 0) {
                            if (seq.remain() < num) {
                                seq.rptr = inipos;
                                expect = num;
                                return State::running;
                            }
                            if (!helper::read_n(result, seq, num)) {
                                return State::failed;
                            }
                            continue;
                        }
                        helper::match_eol(seq);
                        if (num == 0) {
                            return State::complete;
                        }
                    }
                }
                else if (type == BodyType::content_length) {
                    if (seq.remain() < expect) {
                        return State::running;
                    }
                    if (!helper::read_n(result, seq, expect)) {
                        return State::failed;
                    }
                    return State::complete;
                }
                else {
                    if (!helper::read_all(result, seq)) {
                        return State::failed;
                    }
                    return State::complete;
                }
            }

            constexpr auto bodyinfo_preview(BodyType& type, size_t& expect) {
                return [&](auto& key, auto& value) {
                    if (helper::equal(key, "transfer-encoding", helper::ignore_case()) &&
                        helper::contains(value, "chunked")) {
                        type = h1body::BodyType::chuncked;
                    }
                    else if (type == h1body::BodyType::no_info &&
                             helper::equal(key, "content-length", helper::ignore_case())) {
                        auto seq = make_ref_seq(value);
                        number::parse_integer(seq, expect);
                        type = h1body::BodyType::content_length;
                    }
                };
            }
        }  // namespace h1body

    }  // namespace net
}  // namespace utils

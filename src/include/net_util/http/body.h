/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// body - http body reading
#pragma once

#include "../../core/sequencer.h"
#include "../../helper/readutil.h"
#include "../../number/parse.h"
#include "../../helper/equal.h"
#include "../../number/to_string.h"
#include "../../view/sized.h"

namespace utils {
    namespace net {
        namespace h1body {
            enum class BodyType {
                no_info,
                chuncked,
                content_length,
            };

            // read_body reads http body with BodyType
            // Retrun Value
            // 1 - full of body read
            // -1 - invalid body format or length
            // 0 - reading http body is incomplete
            template <class String, class T>
            int read_body(String& result, Sequencer<T>& seq, size_t& expect, BodyType type = BodyType::no_info) {
                auto inipos = seq.rptr;
                if (type == BodyType::chuncked) {
                    while (true) {
                        helper::match_eol(seq);
                        if (seq.eos()) {
                            return 0;
                        }
                        size_t num = 0;
                        auto e = number::parse_integer(seq, num, 16);
                        if (e != number::NumError::none) {
                            return -1;
                        }
                        if (!helper::match_eol(seq)) {
                            return -1;
                        }
                        if (num != 0) {
                            if (seq.remain() < num) {
                                seq.rptr = inipos;
                                expect = num;
                                return 0;
                            }
                            if (!helper::read_n(result, seq, num)) {
                                return -1;
                            }
                            continue;
                        }
                        helper::match_eol(seq);
                        if (num == 0) {
                            return 1;
                        }
                    }
                }
                else if (type == BodyType::content_length) {
                    if (seq.remain() < expect) {
                        return 0;
                    }
                    if (!helper::read_n(result, seq, expect)) {
                        return -1;
                    }
                    return 1;
                }
                else {
                    if (!helper::read_all(result, seq)) {
                        return -1;
                    }
                    return 1;
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

            template <class Buf, class Data = const char*>
            void render_chuncked_data(Buf& buf, Data&& data = "", size_t len = 0) {
                number::to_string(buf, len, 16);
                helper::append(buf, "\r\n");
                helper::append(buf, view::SizedView(data, len));
                helper::append(buf, "\r\n");
            }
        }  // namespace h1body
    }      // namespace net
}  // namespace utils

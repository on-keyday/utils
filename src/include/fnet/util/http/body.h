/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// body - http body reading
#pragma once

#include <core/sequencer.h>
#include <strutil/readutil.h>
#include <strutil/eol.h>
#include <number/parse.h>
#include <strutil/equal.h>
#include <number/to_string.h>
#include <view/sized.h>

namespace futils {
    namespace http {
        namespace body {
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
            constexpr int read_body(String& result, Sequencer<T>& seq, size_t& expect, BodyType type = BodyType::no_info) {
                auto inipos = seq.rptr;
                if (type == BodyType::chuncked) {
                    while (true) {
                        strutil::parse_eol<true>(seq);
                        if (seq.eos()) {
                            return 0;
                        }
                        size_t num = 0;
                        auto e = number::parse_integer(seq, num, 16);
                        if (e != number::NumError::none) {
                            return -1;
                        }
                        if (!strutil::parse_eol<true>(seq)) {
                            return -1;
                        }
                        if (num != 0) {
                            if (seq.remain() < num) {
                                seq.rptr = inipos;
                                expect = num;
                                return 0;
                            }
                            if (!strutil::read_n(result, seq, num)) {
                                return -1;
                            }
                            continue;
                        }
                        strutil::parse_eol<true>(seq);
                        if (num == 0) {
                            return 1;
                        }
                    }
                }
                else if (type == BodyType::content_length) {
                    if (seq.remain() < expect) {
                        return 0;
                    }
                    if (!strutil::read_n(result, seq, expect)) {
                        return -1;
                    }
                    return 1;
                }
                else {
                    if (!strutil::read_all(result, seq)) {
                        return -1;
                    }
                    return 1;
                }
            }

            // HTTPBodyInfo is body information of HTTP
            // this is required for HTTP.body_read
            // and also can get value by HTTP.read_response or HTTP.read_request
            struct HTTPBodyInfo {
                BodyType type = BodyType::no_info;
                size_t expect = 0;
            };

            constexpr auto body_info_preview(BodyType& type, size_t& expect) {
                return [&](auto& key, auto& value) {
                    if (strutil::equal(key, "transfer-encoding", strutil::ignore_case()) &&
                        strutil::contains(value, "chunked")) {
                        type = body::BodyType::chuncked;
                    }
                    else if (type == body::BodyType::no_info &&
                             strutil::equal(key, "content-length", strutil::ignore_case())) {
                        auto seq = make_ref_seq(value);
                        number::parse_integer(seq, expect);
                        type = body::BodyType::content_length;
                    }
                };
            }

            template <class Buf, class Data = const char*>
            void render_chunked_data(Buf& buf, Data&& data = "", size_t len = 0) {
                number::to_string(buf, len, 16);
                strutil::append(buf, "\r\n");
                strutil::append(buf, view::SizedView(data, len));
                strutil::append(buf, "\r\n");
            }
        }  // namespace body
    }      // namespace http
}  // namespace futils

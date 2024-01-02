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
                chunked,
                content_length,
            };

            enum class BodyReadResult {
                full,         // full of body read
                best_effort,  // best effort read because no content-length or transfer-encoding: chunked
                incomplete,   // incomplete read because of no more data
                chunk_read,   // 1 or more chunk read but not full
                invalid,      // invalid body format
            };

            enum class ChunkReadState {
                chunk_size,
                chunk_data,
                chunk_data_line,
                chunk_end,
            };

            // HTTPBodyInfo is body information of HTTP
            // this is required for HTTP.body_read
            // and also can get value by HTTP.read_response or HTTP.read_request
            struct HTTPBodyInfo {
                BodyType type = BodyType::no_info;
                size_t expect = 0;
                ChunkReadState chunk_state = ChunkReadState::chunk_size;
            };

            // read_body reads http body with BodyType
            template <class String, class T>
            constexpr BodyReadResult read_body(String& result, Sequencer<T>& seq, HTTPBodyInfo& info) {
                auto base_pos = seq.rptr;
                if (info.type == BodyType::chunked) {
                    bool read_chunk = false;
                    while (true) {
                        if (seq.eos()) {
                            return BodyReadResult::incomplete;
                        }
                        switch (info.chunk_state) {
                            case ChunkReadState::chunk_size: {
                                base_pos = seq.rptr;
                                size_t num = 0;
                                auto e = number::parse_integer(seq, num, 16);
                                if (e != number::NumError::none) {
                                    return BodyReadResult::invalid;
                                }
                                if (seq.remain() < 2) {
                                    seq.rptr = base_pos;
                                    return BodyReadResult::incomplete;
                                }
                                if (!seq.seek_if("\r\n")) {
                                    return BodyReadResult::invalid;
                                }
                                info.expect = num;
                                info.chunk_state = ChunkReadState::chunk_data;
                                [[fallthrough]];  // fallthrough
                            }
                            case ChunkReadState::chunk_data: {
                                if (seq.remain() < info.expect) {
                                    return BodyReadResult::incomplete;
                                }
                                if (!strutil::read_n(result, seq, info.expect)) {
                                    return BodyReadResult::invalid;
                                }
                                info.chunk_state = ChunkReadState::chunk_data_line;
                                read_chunk = true;
                                [[fallthrough]];  // fallthrough
                            }
                            case ChunkReadState::chunk_data_line: {
                                if (seq.remain() < 2) {
                                    return read_chunk ? BodyReadResult::chunk_read : BodyReadResult::incomplete;
                                }
                                if (!seq.seek_if("\r\n")) {
                                    return BodyReadResult::invalid;
                                }
                                if (info.expect == 0) {
                                    info.chunk_state = ChunkReadState::chunk_end;
                                    return BodyReadResult::full;
                                }
                                info.chunk_state = ChunkReadState::chunk_size;
                                break;
                            }
                            case ChunkReadState::chunk_end: {
                                return BodyReadResult::full;
                            }
                        }
                    }
                }
                else if (info.type == BodyType::content_length) {
                    if (seq.remain() < info.expect) {
                        return BodyReadResult::incomplete;
                    }
                    if (!strutil::read_n(result, seq, info.expect)) {
                        return BodyReadResult::invalid;
                    }
                    return BodyReadResult::full;
                }
                else {
                    if (!strutil::read_all(result, seq)) {
                        return BodyReadResult::invalid;
                    }
                    return BodyReadResult::best_effort;
                }
            }

            constexpr auto body_info_preview(BodyType& type, size_t& expect) {
                return [&](auto& key, auto& value) {
                    if (strutil::equal(key, "transfer-encoding", strutil::ignore_case()) &&
                        strutil::contains(value, "chunked")) {
                        type = body::BodyType::chunked;
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

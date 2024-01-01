/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "cast.h"

namespace futils {
    namespace fnet::quic::frame {

        // parse_frame parses b as a frame and invoke cb with parsed frame
        // cb is void(Frame& frame,bool err) or bool(Frame& frame,bool err)
        // if callback err parameter is true frame parse is incomplete or failed
        // unknown type frame is passed to cb with packed as Frame with err==true
        // if suceeded parse_frame returns true otherwise false
        template <template <class...> class Vec>  // for ACK Frame
        constexpr bool parse_frame(binary::reader& r, auto&& cb) {
            auto invoke_cb = [&](auto& frame, bool err) {
                if constexpr (std::is_convertible_v<decltype(cb(frame, err)), bool>) {
                    if (!cb(frame, err)) {
                        return false;
                    }
                }
                else {
                    cb(frame, err);
                }
                return true;
            };
            auto peek = r.peeker();
            Frame f;
            if (!f.parse(peek)) {
                return false;
            }
            return select_Frame<Vec>(f.type.type(), [&](auto frame) {
                if constexpr (std::is_same_v<decltype(frame), Frame>) {
                    cb(f, true);
                    return false;
                }
                auto res = frame.parse(r);
                if (!res) {
                    invoke_cb(frame, true);
                    return false;
                }
                return invoke_cb(frame, false);
            });
        }

        // parse_frames parses b as frames
        // cb is same as parse_frame function parameter
        // PADDING frame is ignored and doesn't elicit cb invocation if ignore_padding is true
        // limit is reading frame count limit
        template <template <class...> class Vec>
        constexpr bool parse_frames(binary::reader& r, auto&& cb, size_t limit = ~0, bool ignore_padding = true) {
            size_t i = 0;
            while (!r.empty() && i < limit) {
                if (ignore_padding && r.top() == 0) {
                    r.offset(1);
                    continue;  // PADDING frame, ignore
                }
                if (!parse_frame<Vec>(r, cb)) {
                    return false;
                }
                i++;
            }
            return true;
        }

        namespace test {
            constexpr bool check_parse() {
                byte data[113]{};
                byte dummy[5]{'h', 'e', 'l', 'l', 'o'};
                binary::writer w{data};
                for (auto type : known_frame_types) {
                    bool ok = select_Frame<quic::test::FixedTestVec>(type, [&](auto&& frame) {
                        if (type == FrameType::STREAMS_BLOCKED ||
                            type == FrameType::MAX_STREAMS) {
                            frame.type = type;
                        }
                        frame.visit_rvec([&](auto& rep) {
                            rep = dummy;
                        });
                        return frame.render(w);
                    });
                    if (!ok) {
                        return false;
                    }
                }
                binary::reader r{w.written()};
                size_t i = 0;
                return parse_frames<quic::test::FixedTestVec>(
                    r, [&](auto&& fr, bool) {
                        return fr.type.type() == known_frame_types[i++];
                    },
                    -1, false);
            }

            static_assert(check_parse());
        }  // namespace test

    }  // namespace fnet::quic::frame
}  // namespace futils

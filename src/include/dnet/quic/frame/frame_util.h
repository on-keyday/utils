/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame_util - QUIC frame utility
#pragma once
#include "frame.h"

namespace utils {
    namespace dnet {
        namespace quic::frame {

            constexpr auto frame_type_to_Type(FrameType f, auto&& cb) {
#define PARSE(frame_type, TYPE) \
    if (f == frame_type) {      \
        return cb(TYPE{});      \
    }
                PARSE(FrameType::PADDING, PaddingFrame)
                PARSE(FrameType::PING, PingFrame)
                PARSE(FrameType::ACK, ACKFrame)
                PARSE(FrameType::RESET_STREAM, ResetStreamFrame)
                PARSE(FrameType::STOP_SENDING, StopSendingFrame)
                PARSE(FrameType::CRYPTO, CryptoFrame)
                PARSE(FrameType::NEW_TOKEN, NewTokenFrame)
                PARSE(FrameType::STREAM, StreamFrame)
                PARSE(FrameType::MAX_DATA, MaxDataFrame)
                PARSE(FrameType::MAX_STREAM_DATA, MaxStreamDataFrame)
                PARSE(FrameType::MAX_STREAMS, MaxStreamsFrame)
                PARSE(FrameType::DATA_BLOCKED, DataBlockedFrame)
                PARSE(FrameType::STREAM_DATA_BLOCKED, StreamDataBlockedFrame)
                PARSE(FrameType::STREAMS_BLOCKED, StreamsBlockedFrame)
                PARSE(FrameType::NEW_CONNECTION_ID, NewConnectionIDFrame)
                PARSE(FrameType::RETIRE_CONNECTION_ID, RetireConnectionIDFrame)
                PARSE(FrameType::PATH_CHALLENGE, PathChallengeFrame)
                PARSE(FrameType::PATH_RESPONSE, PathResponseFrame)
                PARSE(FrameType::CONNECTION_CLOSE, ConnectionCloseFrame)
                PARSE(FrameType::HANDSHAKE_DONE, HandshakeDoneFrame)
                PARSE(FrameType::DATAGRAM, DatagramFrame)
#undef PARSE
                return cb(Frame{});
            }

            // parse_frame parses b as a frame and invoke cb with parsed frame
            // cb is void(Frame& frame,bool err) or bool(Frame& frame,bool err)
            // if callback err parameter is true frame parse is incomplete or failed
            // unknown type frame is passed to cb with packed as Frame with err==true
            // if suceeded parse_frame returns true otherwise false
            constexpr bool parse_frame(ByteLen& b, auto&& cb) {
                if (!b.enough(1)) {
                    return false;
                }
                auto invoke_cb = [&](auto& frame, bool err) {
                    using Result = std::invoke_result_t<decltype(cb), decltype(frame), bool>;
                    if constexpr (internal::has_logical_not<Result>) {
                        if (!cb(frame, err)) {
                            return false;
                        }
                    }
                    else {
                        cb(frame, err);
                    }
                    return true;
                };
                return frame_type_to_Type(FrameFlags{b.data}.type(), [&](auto frame) {
                    auto res = frame.parse(b);
                    if constexpr (std::is_same_v<decltype(frame), Frame>) {
                        cb(frame, true);
                        return false;
                    }
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
            constexpr bool parse_frames(ByteLen& b, auto&& cb, size_t limit = ~0, bool ignore_padding = true) {
                size_t i = 0;
                while (b.enough(1) && i < limit) {
                    if (ignore_padding && *b.data == 0) {
                        b = b.forward(1);
                        continue;  // PADDING frame, ignore
                    }
                    if (!parse_frame(b, cb)) {
                        return false;
                    }
                    i++;
                }
                return true;
            }

            template <class F>
            constexpr F* frame_cast(Frame* f) {
                if (!f || !f->type.valid()) {
                    return nullptr;
                }
                return frame_type_to_Type(f->type.type(), [&](auto frame) -> F* {
                    if constexpr (std::is_same_v<decltype(frame), F>) {
                        return static_cast<F*>(f);
                    }
                    return nullptr;
                });
            }

            constexpr auto wpacket_frame_vec(WPacket& src, auto& vec, FrameType* errtype) {
                return [&, errtype](auto& fr, bool err) {
                    if (err) {
                        if (errtype) {
                            *errtype = fr.type.type();
                        }
                        return false;
                    }
                    auto aq = src.aligned_aquire(sizeof(fr), alignof(decltype(fr)));
                    if (!aq.enough(sizeof(fr))) {
                        return false;
                    }
                    void* vtyp = aq.data;
                    auto frame = static_cast<std::remove_cvref_t<decltype(fr)>*>(vtyp);
                    *frame = fr;
                    vec.push_back(frame);
                    return true;
                };
            }

            constexpr size_t sizeof_frame(FrameType type) {
                byte b = byte(type);
                auto ct = FrameFlags{&b}.type();
                return frame_type_to_Type(ct, [&](auto f) -> size_t {
                    return sizeof(f);
                });
            }

        }  // namespace quic::frame
    }      // namespace dnet
}  // namespace utils

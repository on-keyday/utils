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
        namespace quic {

            template <class From, class To>
            concept convertible = std::is_convertible_v<From, To>;

            template <class T>
            concept has_logical_not = requires(T t) {
                { !t } -> convertible<bool>;
            };

            bool parse_frame(ByteLen& b, auto&& cb) {
                if (!b.enough(1)) {
                    return false;
                }
                auto typ = FrameFlags{b.data}.type();
                auto invoke_cb = [&](auto& frame) {
                    using Result = std::invoke_result_t<decltype(cb), decltype(frame)>;
                    if constexpr (has_logical_not<Result>) {
                        if (!cb(frame)) {
                            return false;
                        }
                    }
                    else {
                        cb(frame);
                    }
                    return true;
                };
#define PARSE(frame_type, TYPE)  \
    if (typ == frame_type) {     \
        TYPE frame;              \
        if (!frame.parse(b)) {   \
            return false;        \
        }                        \
        return invoke_cb(frame); \
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
                Frame fr;
                fr.parse(b);
                cb(fr);
                return false;
            }

            bool parse_frames(ByteLen& b, auto&& cb) {
                while (b.enough(1)) {
                    if (*b.data == 0) {
                        b = b.forward(1);
                        continue;  // PADDING frame, ignore
                    }
                    if (!parse_frame(b, cb)) {
                        return false;
                    }
                }
                return true;
            }
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

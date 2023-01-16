/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame_make - make frame
#pragma once
#include "frame.h"

namespace utils {
    namespace dnet {
        namespace quic::frame {
            template <class T, FrameType type>
            constexpr T make_one_byte_frame(WPacket* src) {
                T frame;
                frame.type = FrameFlags{type};
                if (src) {
                    src->frame_type(frame.type.type_detail());
                }
                return frame;
            }

            constexpr PaddingFrame make_padding(WPacket* src) {
                return make_one_byte_frame<PaddingFrame, FrameType::PADDING>(src);
            }

            constexpr PingFrame make_ping(WPacket* src) {
                return make_one_byte_frame<PingFrame, FrameType::PING>(src);
            }

            constexpr std::pair<ACKFrame, error::Error> make_ack(WPacket& src, size_t ack_delay, auto&& ack_ranges, ECNCounts* counts = nullptr) {
                if (auto err = validate_ack_ranges(ack_ranges)) {
                    return {{}, err};
                }
                ACKFrame frame;
                frame.type = FrameFlags(counts ? FrameType::ACK_ECN : FrameType::ACK);
                src.frame_type(frame.type.type_detail());
                size_t i = 0;
                size_t first_ack_range = 0;
                size_t largest = 0;
                size_t offset = 0;
                ACKRange prev{};
                for (ACKRange range : ack_ranges) {
                    if (i == 0) {
                        first_ack_range = range.largest - range.smallest;
                        largest = range.largest;
                        prev = range;
                        frame.largest_ack = largest;
                        frame.ack_delay = ack_delay;
                        frame.ack_range_count = ack_ranges.size() - 1;
                        frame.first_ack_range = first_ack_range;
                        src.qvarint(largest);
                        src.qvarint(ack_delay);
                        src.qvarint(frame.ack_range_count);
                        src.qvarint(first_ack_range);
                    }
                    else {
                        if (i == 1) {
                            offset = src.offset;
                        }
                        auto gap = prev.smallest - range.largest - 2;
                        auto ack_range = range.largest - range.smallest;
                        src.qvarint(gap);
                        src.qvarint(ack_range);
                        prev = range;
                    }
                    i++;
                }
                if (src.overflow) {
                    return {{}, error::Error("wpacket overflow", error::ErrorCategory::validationerr)};
                }
                if (i > 1) {
                    auto len = src.offset - offset;
                    src.offset = offset;
                    frame.ack_ranges = src.acquire(len);
                }
                if (counts) {
                    offset = src.offset;
                    src.qvarint(counts->ect0);
                    src.qvarint(counts->ect1);
                    src.qvarint(counts->ecn_ce);
                    if (src.overflow) {
                        return {{}, error::Error("wpacket overflow", error::ErrorCategory::validationerr)};
                    }
                    auto len = src.offset - offset;
                    frame.ecn_counts = src.acquire(len);
                }
                return {frame, error::none};
            }

            constexpr ResetStreamFrame make_reset_stream(WPacket* src, size_t stream_id, size_t errcode, size_t final_size) {
                ResetStreamFrame frame;
                frame.type = FrameFlags(FrameType::RESET_STREAM);
                frame.streamID = stream_id;
                frame.application_protocol_error_code = errcode;
                frame.final_size = final_size;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr StopSendingFrame make_stop_sending(WPacket* src, size_t stream_id, size_t errcode) {
                StopSendingFrame frame;
                frame.type = FrameFlags(FrameType::STOP_SENDING);
                frame.streamID = stream_id;
                frame.application_protocol_error_code = errcode;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr CryptoFrame make_crypto(WPacket* src, size_t offset, ByteLen data) {
                CryptoFrame frame;
                frame.type = FrameFlags(FrameType::CRYPTO);
                frame.offset = offset;
                frame.crypto_data = data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr NewTokenFrame make_new_token(WPacket* src, ByteLen token) {
                NewTokenFrame frame;
                frame.type = FrameFlags(FrameType::NEW_TOKEN);
                frame.token = token;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr StreamFrame make_stream(WPacket* src, size_t stream_id, ByteLen data,
                                              bool len = true, bool fin = false, size_t offset = 0) {
                StreamFrame frame;
                frame.type = FrameFlags(FrameType::STREAM);
                auto& v = frame.type.value;
                frame.streamID = stream_id;
                frame.offset = offset;
                if (offset != 0) {
                    v |= 0x04;
                }
                if (len) {
                    v |= 0x02;
                }
                if (fin) {
                    v |= 0x01;
                }
                frame.stream_data = data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr MaxDataFrame make_max_data(WPacket* src, size_t max_data) {
                MaxDataFrame frame;
                frame.type = FrameFlags(FrameType::MAX_DATA);
                frame.max_data = max_data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr MaxStreamDataFrame make_max_stream_data(WPacket* src, size_t stream_id, size_t max_data) {
                MaxStreamDataFrame frame;
                frame.type = FrameFlags(FrameType::MAX_STREAM_DATA);
                frame.streamID = stream_id;
                frame.max_stream_data = max_data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr MaxStreamsFrame make_max_streams(WPacket* src, bool uni, size_t max_streams) {
                MaxStreamsFrame frame;
                frame.type = FrameFlags(uni ? FrameType::MAX_STREAMS_UNI : FrameType::MAX_STREAMS_BIDI);
                frame.max_streams = max_streams;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr DataBlockedFrame make_data_blocked(WPacket* src, size_t max_data) {
                DataBlockedFrame frame;
                frame.type = FrameFlags(FrameType::DATA_BLOCKED);
                frame.max_data = max_data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr StreamDataBlockedFrame make_stream_data_blocked(WPacket* src, size_t stream_id, size_t max_data) {
                StreamDataBlockedFrame frame;
                frame.type = FrameFlags(FrameType::STREAM_DATA_BLOCKED);
                frame.streamID = stream_id;
                frame.max_stream_data = max_data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr StreamsBlockedFrame make_streams_blocked(WPacket* src, bool uni, size_t max_streams) {
                StreamsBlockedFrame frame;
                frame.type = FrameFlags(uni ? FrameType::STREAMS_BLOCKED_UNI : FrameType::STREAMS_BLOCKED_BIDI);
                frame.max_streams = max_streams;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr NewConnectionIDFrame make_new_connection_id(
                WPacket* src, size_t seq_number, size_t retire_proior_to,
                ByteLen new_id, ByteLen state_less_reset) {
                if (!state_less_reset.data || state_less_reset.len != 16) {
                    return {};
                }
                NewConnectionIDFrame frame;
                frame.type = FrameFlags(FrameType::NEW_CONNECTION_ID);
                frame.squecne_number = seq_number;
                frame.retire_proior_to = retire_proior_to;
                frame.connectionID = new_id;
                frame.stateless_reset_token = state_less_reset;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr RetireConnectionIDFrame make_retire_connection_id(WPacket* src, size_t seq_number) {
                RetireConnectionIDFrame frame;
                frame.type = FrameFlags(FrameType::RETIRE_CONNECTION_ID);
                frame.sequence_number = seq_number;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr PathChallengeFrame make_path_challange(WPacket* src, std::uint64_t data) {
                PathChallengeFrame frame;
                frame.type = FrameFlags(FrameType::PATH_CHALLENGE);
                frame.data = data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr PathResponseFrame make_path_response(WPacket* src, std::uint64_t data) {
                PathResponseFrame frame;
                frame.type = FrameFlags(FrameType::PATH_RESPONSE);
                frame.data = data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

            constexpr ConnectionCloseFrame make_connection_close(WPacket& src, FrameType type, bool app, size_t errcode, ConstByteLen reason) {
                ConnectionCloseFrame frame;
                frame.type = FrameFlags(app ? FrameType::CONNECTION_CLOSE_APP : FrameType::CONNECTION_CLOSE);
                frame.error_code = errcode;
                if (!app) {
                    frame.frame_type = size_t(type);
                }
                src.frame_type(frame.type.type_detail());
                src.qvarint(frame.error_code);
                if (!app) {
                    src.qvarint(frame.frame_type);
                }
                frame.reason_phrase = src.copy_from(reason);
                return frame;
            }

            constexpr HandshakeDoneFrame make_handshake_done(WPacket* src) {
                return make_one_byte_frame<HandshakeDoneFrame, FrameType::HANDSHAKE_DONE>(src);
            }

            constexpr DatagramFrame make_datagrm(WPacket* src, ByteLen data, bool len) {
                DatagramFrame frame;
                frame.type = FrameFlags(len ? FrameType::DATAGRAM_LEN : FrameType::DATAGRAM);
                frame.datagram_data = data;
                if (src) {
                    frame.render(*src);
                }
                return frame;
            }

        }  // namespace quic::frame
    }      // namespace dnet
}  // namespace utils

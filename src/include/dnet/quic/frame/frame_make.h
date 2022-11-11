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
            constexpr T make_one_byte_frame(WPacket& src) {
                T frame;
                frame.type = src.frame_type(type);
                return frame;
            }

            constexpr PaddingFrame make_padding(WPacket& src) {
                return make_one_byte_frame<PaddingFrame, FrameType::PADDING>(src);
            }

            constexpr PingFrame make_ping(WPacket& src) {
                return make_one_byte_frame<PingFrame, FrameType::PING>(src);
            }

            constexpr ACKFrame make_ack(WPacket& src, size_t ack_delay, auto&& ack_ranges, ECNCounts* counts = nullptr) {
                if (!validate_ack_ranges(ack_ranges)) {
                    return {};
                }
                ACKFrame frame;
                frame.type = src.frame_type(counts ? FrameType::ACK_ECN : FrameType::ACK);
                size_t i = 0;
                size_t first_ack_range = 0;
                size_t largest = 0;
                size_t offset = src.offset;
                ACKRange prev{};
                for (ACKRange range : ack_ranges) {
                    if (i == 0) {
                        first_ack_range = range.largest - range.smallest;
                        largest = range.largest;
                        prev = range;
                        frame.largest_ack = src.qvarint(largest);
                        frame.ack_delay = src.qvarint(ack_delay);
                        frame.ack_range_count = src.qvarint(ack_ranges.size() - 1);
                        frame.first_ack_range = src.qvarint(first_ack_range);
                    }
                    else {
                        auto gap = prev.smallest - range.largest - 2;
                        auto ack_range = range.largest - range.smallest;
                        src.qvarint(gap);
                        src.qvarint(ack_range);
                        prev = range;
                    }
                    i++;
                }
                if (src.overflow) {
                    return {};
                }
                auto len = src.offset - offset;
                src.offset = offset;
                frame.ack_ranges = src.acquire(len);
                if (counts) {
                    offset = src.offset;
                    src.qvarint(counts->ect0);
                    src.qvarint(counts->ect1);
                    src.qvarint(counts->ecn_ce);
                    if (src.overflow) {
                        return {};
                    }
                    len = src.offset - offset;
                    frame.ecn_counts = src.acquire(len);
                }
                return frame;
            }

            constexpr ResetStreamFrame make_reset_stream(WPacket& src, size_t stream_id, size_t errcode, size_t final_size) {
                ResetStreamFrame frame;
                frame.type = src.frame_type(FrameType::RESET_STREAM);
                frame.streamID = src.qvarint(stream_id);
                frame.application_protocol_error_code = src.qvarint(errcode);
                frame.fianl_size = src.qvarint(final_size);
                return frame;
            }

            constexpr StopSendingFrame make_stop_sending(WPacket& src, size_t stream_id, size_t errcode) {
                StopSendingFrame frame;
                frame.type = src.frame_type(FrameType::STOP_SENDING);
                frame.streamID = src.qvarint(stream_id);
                frame.application_protocol_error_code = src.qvarint(errcode);
                return frame;
            }

            constexpr CryptoFrame make_crypto(WPacket& src, size_t offset, ByteLen data, bool copy = true) {
                CryptoFrame frame;
                frame.type = src.frame_type(FrameType::CRYPTO);
                frame.offset = src.qvarint(offset);
                frame.length = src.qvarint(data.len);
                if (copy) {
                    frame.crypto_data = src.copy_from(data);
                }
                else {
                    frame.crypto_data = data;
                }
                return frame;
            }

            constexpr NewTokenFrame make_new_token(WPacket& src, ByteLen token, bool copy = true) {
                NewTokenFrame frame;
                frame.type = src.frame_type(FrameType::NEW_TOKEN);
                frame.token_length = src.qvarint(token.len);
                if (copy) {
                    frame.token = src.copy_from(token);
                }
                else {
                    frame.token = token;
                }
                return frame;
            }

            constexpr StreamFrame make_stream(WPacket& src, size_t stream_id, ByteLen data,
                                              bool len = true, bool fin = false, size_t offset = ~0,
                                              bool copy = true) {
                StreamFrame frame;
                frame.type = src.frame_type(FrameType::STREAM);
                if (!frame.type.valid()) {
                    return {};
                }
                auto& v = *frame.type.value;
                frame.streamID = src.qvarint(stream_id);
                if (offset < (size_t(0x40) << (8 * 7))) {
                    frame.offset = src.qvarint(offset);
                    v |= 0x04;
                }
                if (len) {
                    frame.length = src.qvarint(data.qvarint());
                    v |= 0x02;
                }
                if (fin) {
                    v |= 0x01;
                }
                if (copy) {
                    frame.stream_data = src.copy_from(data);
                }
                else {
                    frame.stream_data = data;
                }
                return frame;
            }

            constexpr MaxDataFrame make_max_data(WPacket& src, size_t max_data) {
                MaxDataFrame frame;
                frame.type = src.frame_type(FrameType::MAX_DATA);
                frame.max_data = src.qvarint(max_data);
                return frame;
            }

            constexpr MaxStreamDataFrame make_max_stream_data(WPacket& src, size_t stream_id, size_t max_data) {
                MaxStreamDataFrame frame;
                frame.type = src.frame_type(FrameType::MAX_STREAM_DATA);
                frame.streamID = src.qvarint(stream_id);
                frame.max_stream_data = src.qvarint(max_data);
                return frame;
            }

            constexpr MaxStreamsFrame make_max_streams(WPacket& src, bool uni, size_t max_streams) {
                MaxStreamsFrame frame;
                frame.type = src.frame_type(uni ? FrameType::MAX_STREAMS_UNI : FrameType::MAX_STREAMS_BIDI);
                frame.max_streams = src.qvarint(max_streams);
                return frame;
            }

            constexpr DataBlockedFrame make_data_blocked(WPacket& src, size_t max_data) {
                DataBlockedFrame frame;
                frame.type = src.frame_type(FrameType::DATA_BLOCKED);
                frame.max_data = src.qvarint(max_data);
                return frame;
            }

            constexpr StreamDataBlockedFrame make_stream_data_blocked(WPacket& src, size_t stream_id, size_t max_data) {
                StreamDataBlockedFrame frame;
                frame.type = src.frame_type(FrameType::STREAM_DATA_BLOCKED);
                frame.streamID = src.qvarint(stream_id);
                frame.max_stream_data = src.qvarint(max_data);
                return frame;
            }

            constexpr StreamsBlockedFrame make_streams_blocked(WPacket& src, bool uni, size_t max_streams) {
                StreamsBlockedFrame frame;
                frame.type = src.frame_type(uni ? FrameType::STREAMS_BLOCKED_UNI : FrameType::STREAMS_BLOCKED_BIDI);
                frame.max_streams = src.qvarint(max_streams);
                return frame;
            }

            constexpr NewConnectionIDFrame make_new_connection_id(
                WPacket& src, size_t seq_number, size_t retire_proior_to,
                ByteLen new_id, ByteLen state_less_reset, bool copy = true) {
                if (!state_less_reset.enough(16)) {
                    return {};
                }
                NewConnectionIDFrame frame;
                frame.type = src.frame_type(FrameType::NEW_CONNECTION_ID);
                frame.squecne_number = src.qvarint(seq_number);
                frame.retire_proior_to = src.qvarint(retire_proior_to);
                frame.length = src.as_byte(new_id.len);
                if (copy) {
                    frame.connectionID = src.copy_from(new_id);
                    frame.stateless_reset_token = src.copy_from(state_less_reset.resized(16));
                }
                else {
                    frame.connectionID = new_id;
                    frame.stateless_reset_token = state_less_reset.resized(16);
                }
                return frame;
            }

            constexpr RetireConnectionIDFrame make_retire_connection_id(WPacket& src, size_t seq_number) {
                RetireConnectionIDFrame frame;
                frame.type = src.frame_type(FrameType::RETIRE_CONNECTION_ID);
                frame.sequence_number = src.qvarint(seq_number);
                return frame;
            }

            constexpr PathChallengeFrame make_path_challange(WPacket& src, std::uint64_t data) {
                PathChallengeFrame frame;
                frame.type = src.frame_type(FrameType::PATH_CHALLENGE);
                frame.data = src.as(data);
                return frame;
            }

            constexpr PathResponseFrame make_path_response(WPacket& src, std::uint64_t data) {
                PathResponseFrame frame;
                frame.type = src.frame_type(FrameType::PATH_RESPONSE);
                frame.data = src.as(data);
                return frame;
            }

            constexpr ConnectionCloseFrame make_connection_close(WPacket& src, FrameType type, bool app, size_t errcode, ConstByteLen reason) {
                ConnectionCloseFrame frame;
                frame.type = src.frame_type(app ? FrameType::CONNECTION_CLOSE_APP : FrameType::CONNECTION_CLOSE);
                frame.error_code = src.qvarint(errcode);
                if (!app) {
                    frame.frame_type = src.qvarint(size_t(type));
                }
                frame.reason_phrase_length = src.qvarint(reason.len);
                frame.reason_phrase = src.copy_from(reason);
                return frame;
            }

            constexpr HandshakeDoneFrame make_handshake_done(WPacket& src) {
                return make_one_byte_frame<HandshakeDoneFrame, FrameType::HANDSHAKE_DONE>(src);
            }

            constexpr DatagramFrame make_datagrm(WPacket& src, ByteLen data, bool len, bool copy = true) {
                DatagramFrame frame;
                frame.type = src.frame_type(len ? FrameType::DATAGRAM_LEN : FrameType::DATAGRAM);
                if (len) {
                    frame.length = src.qvarint(data.len);
                }
                if (copy) {
                    frame.datagram_data = src.copy_from(data);
                }
                else {
                    frame.datagram_data = data;
                }
                return frame;
            }

        }  // namespace quic::frame
    }      // namespace dnet
}  // namespace utils

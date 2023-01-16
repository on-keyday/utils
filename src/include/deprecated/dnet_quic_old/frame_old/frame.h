/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame - QUIC frame format
#pragma once
#include "frame_base.h"
#include "../../../bytelen.h"
#include "../../../error.h"

namespace utils {
    namespace dnet {
        namespace quic::frame {

            struct PaddingFrame : Frame {
                constexpr bool parse(ByteLen& b) {
                    return Frame::parse(b) &&
                           type.type() == FrameType::PADDING;
                }

                constexpr size_t parse_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    return type.type() == FrameType::PADDING &&
                           Frame::render(w);
                }

                constexpr size_t render_len() const {
                    return 1;
                }
            };

            struct PingFrame : Frame {
                constexpr bool parse(ByteLen& b) {
                    return Frame::parse(b) &&
                           type.type() == FrameType::PING;
                }

                constexpr size_t parse_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    return type.type() == FrameType::PING &&
                           Frame::render(w);
                }

                constexpr size_t render_len() const {
                    return 1;
                }
            };

            struct ACKRange {
                size_t smallest = 0;
                size_t largest = 0;
            };

            struct ECNCounts {
                size_t ect0 = 0;
                size_t ect1 = 0;
                size_t ecn_ce = 0;
            };

            struct ACKFrame : Frame {
                QVarInt largest_ack;
                QVarInt ack_delay;
                QVarInt ack_range_count;
                QVarInt first_ack_range;
                ByteLen ack_ranges;
                ByteLen ecn_counts;

                // cb is cb(gap,len)
                static constexpr size_t parse_ack_range(ByteLen b, size_t count, auto&& cb) {
                    size_t range_len = 0;
                    for (auto i = 0; i < count; i++) {
                        size_t gap = 0, len = 0;
                        auto first = b.qvarintfwd(gap);
                        if (!first) {
                            return 0;
                        }
                        auto second = b.qvarintfwd(len);
                        if (!second) {
                            return 0;
                        }
                        if constexpr (internal::has_logical_not<decltype(cb(gap, len))>) {
                            if (!cb(gap, len)) {
                                return 0;
                            }
                        }
                        else {
                            cb(gap, len);
                        }

                        range_len += first + second;
                    }
                    return range_len;
                }

                static constexpr size_t parse_ecn_counts(ByteLen b, auto&& cb) {
                    size_t counts_len = 0;
                    size_t ect0 = 0, ect1 = 0, ecn_ce = 0;
                    auto first = b.qvarintfwd(ect0);
                    if (!first) {
                        return 0;
                    }
                    auto second = b.qvarintfwd(ect1);
                    if (!second) {
                        return 0;
                    }
                    auto third = b.qvarintfwd(ecn_ce);
                    if (!third) {
                        return 0;
                    }
                    counts_len = first + second + third;
                    cb(ect0, ect1, ecn_ce);
                    return counts_len;
                }

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::ACK) {
                        return false;
                    }
                    if (!largest_ack.parse(b) ||
                        !ack_delay.parse(b) ||
                        !ack_range_count.parse(b) ||
                        !first_ack_range.parse(b)) {
                        return false;
                    }
                    auto len = parse_ack_range(b, ack_range_count, [](auto...) {});
                    if (ack_range_count != 0 && !len) {
                        return false;
                    }
                    ack_ranges = b.resized(len);
                    b = b.forward(len);
                    if (type.type_detail() == FrameType::ACK_ECN) {
                        len = parse_ecn_counts(b, [](auto...) {});
                        if (!len) {
                            return false;
                        }
                        ecn_counts = b.resized(len);
                        b = b.forward(len);
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    auto v = Frame::parse_len() +
                             largest_ack.len +
                             ack_delay.len +
                             ack_range_count.len +
                             first_ack_range.len +
                             ack_ranges.len;
                    if (type.type_detail() == FrameType::ACK_ECN) {
                        v += ecn_counts.len;
                    }
                    return v;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::ACK) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    auto range_len = parse_ack_range(ack_ranges, ack_range_count, [](auto...) {});
                    if (ack_range_count != 0 && !range_len) {
                        return false;
                    }
                    if (!largest_ack.render(w) ||
                        !ack_delay.render(w) ||
                        !ack_range_count.render(w) ||
                        !first_ack_range.render(w)) {
                        return false;
                    }
                    w.append(ack_ranges.data, range_len);
                    if (type.type_detail() == FrameType::ACK_ECN) {
                        auto ecn_len = parse_ecn_counts(ecn_counts, [](auto...) {});
                        if (!ecn_len) {
                            return false;
                        }
                        w.append(ecn_counts.data, ecn_len);
                    }
                    return true;
                }

                constexpr size_t render_len() const {
                    auto v = Frame::render_len() +
                             largest_ack.minimum_len() +
                             ack_delay.minimum_len() +
                             ack_range_count.minimum_len() +
                             first_ack_range.minimum_len() +
                             ack_ranges.len;
                    if (type.type_detail() == FrameType::ACK_ECN) {
                        v += ecn_counts.len;
                    }
                    return v;
                }

                constexpr size_t visit_bytelen(auto&& cb) {
                    cb(ack_ranges);
                    cb(ecn_counts);
                    return ack_ranges.len + ecn_counts.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    cb(ack_ranges);
                    cb(ecn_counts);
                    return ack_ranges.len + ecn_counts.len;
                }
            };

            constexpr error::Error validate_ack_ranges(auto& vec) {
                bool least_one = false;
                for (ACKRange r : vec) {
                    least_one = true;
                    if (r.smallest > r.largest) {
                        return error::Error("invalid ACKRange.found range.smallest > range.largest", error::ErrorCategory::quicerr);
                    }
                }
                if (!least_one) {
                    return error::Error("no ack range exists", error::ErrorCategory::quicerr);
                }
                for (size_t i = 0; i < vec.size(); i++) {
                    if (i == 0) {
                        continue;
                    }
                    ACKRange r = vec[i];
                    ACKRange last = vec[i - 1];
                    if (last.smallest <= r.smallest || last.largest <= r.largest) {
                        return error::Error("ack ranges are not sorted", error::ErrorCategory::quicerr);
                    }
                }
                return error::none;
            }

            constexpr error::Error get_ackranges(auto& ranges, const ACKFrame& ack) {
                auto largest_ack = ack.largest_ack.value;
                auto ackBlock = ack.first_ack_range.value;
                auto smallest = largest_ack - ackBlock;
                ranges.push_back(ACKRange{smallest, largest_ack});
                if (ack.ack_range_count) {
                    auto res = ACKFrame::parse_ack_range(ack.ack_ranges, ack.ack_range_count, [&](auto gap, auto ackBlock) {
                        if (smallest < gap + 2) {
                            return false;
                        }
                        auto largest = smallest - gap - 2;
                        if (ackBlock > largest) {
                            return false;
                        }
                        smallest = largest - ackBlock;
                        ranges.push_back(ACKRange{smallest, largest});
                        return true;
                    });
                    if (!res) {
                        return error::Error("invalid ack range. ", error::ErrorCategory::quicerr);
                    }
                }
                if (auto err = validate_ack_ranges(ranges)) {
                    return err;
                }
                return error::none;
            }

            struct ResetStreamFrame : Frame {
                QVarInt streamID;
                QVarInt application_protocol_error_code;
                QVarInt final_size;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::RESET_STREAM) {
                        return false;
                    }
                    if (!streamID.parse(b) ||
                        !application_protocol_error_code.parse(b) ||
                        !final_size.parse(b)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() +
                           streamID.len +
                           application_protocol_error_code.len +
                           final_size.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::RESET_STREAM) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!streamID.render(w) ||
                        !application_protocol_error_code.render(w) ||
                        !final_size.render(w)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() +
                           streamID.minimum_len() +
                           application_protocol_error_code.minimum_len() +
                           final_size.minimum_len();
                }
            };

            struct StopSendingFrame : Frame {
                QVarInt streamID;
                QVarInt application_protocol_error_code;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STOP_SENDING) {
                        return false;
                    }
                    if (!streamID.parse(b) ||
                        !application_protocol_error_code.parse(b)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() +
                           streamID.len +
                           application_protocol_error_code.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::STOP_SENDING) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!streamID.render(w) ||
                        !application_protocol_error_code.render(w)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() +
                           streamID.minimum_len() +
                           application_protocol_error_code.minimum_len();
                }
            };

            struct CryptoFrame : Frame {
                QVarInt offset;
                QVarInt length;  // only parse
                ByteLen crypto_data;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::CRYPTO) {
                        return false;
                    }
                    if (!offset.parse(b) ||
                        !length.parse(b)) {
                        return false;
                    }
                    if (!b.enough(length)) {
                        return false;
                    }
                    crypto_data = b.resized(length);
                    b = b.forward(length);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() +
                           offset.len +
                           length.len +
                           crypto_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::CRYPTO) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!offset.render(w)) {
                        return false;
                    }
                    QVarInt len{crypto_data.len};
                    if (!len.render(w)) {
                        return false;
                    }
                    if (!crypto_data.data && crypto_data.len) {
                        return false;
                    }
                    w.append(crypto_data.data, crypto_data.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() +
                           offset.minimum_len() +
                           QVarInt{crypto_data.len}.minimum_len() +
                           crypto_data.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) {
                    cb(crypto_data);
                    return crypto_data.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    cb(crypto_data);
                    return crypto_data.len;
                }
            };

            struct NewTokenFrame : Frame {
                QVarInt token_length;  // only parse
                ByteLen token;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::NEW_TOKEN) {
                        return false;
                    }
                    if (!token_length.parse(b)) {
                        return false;
                    }
                    if (!b.enough(token_length)) {
                        return false;
                    }
                    token = b.resized(token_length);
                    b = b.forward(token_length);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() +
                           token_length.len +
                           token.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::NEW_TOKEN) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!QVarInt{token.len}.render(w)) {
                        return false;
                    }
                    if (!token.data && token.len) {  // avoid nullpo ga!
                        return false;
                    }
                    w.append(token.data, token.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() +
                           QVarInt{token.len}.minimum_len() +
                           token.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) {
                    cb(token);
                    return token.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    cb(token);
                    return token.len;
                }
            };

            struct StreamFrame : Frame {
                QVarInt streamID;
                QVarInt offset;
                QVarInt length;  // on parse
                ByteLen stream_data;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAM) {
                        return false;
                    }
                    if (!streamID.parse(b)) {
                        return false;
                    }
                    if (type.STREAM_off()) {
                        if (!offset.parse(b)) {
                            return false;
                        }
                    }
                    if (type.STREAM_len()) {
                        if (!length.parse(b)) {
                            return false;
                        }
                        if (!b.enough(length)) {
                            return false;
                        }
                        stream_data = b.resized(length);
                        b = b.forward(length);
                    }
                    else {
                        stream_data = b;
                        b = b.forward(b.len);
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    auto v = Frame::parse_len() +
                             streamID.len +
                             stream_data.len;
                    if (type.STREAM_off()) {
                        v += offset.len;
                    }
                    if (type.STREAM_len()) {
                        v += length.len;
                    }
                    return v;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::STREAM) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!streamID.render(w)) {
                        return false;
                    }
                    if (type.STREAM_off()) {
                        if (!offset.render(w)) {
                            return false;
                        }
                    }
                    if (type.STREAM_len()) {
                        if (!QVarInt{stream_data.len}.render(w)) {
                            return false;
                        }
                    }
                    if (!stream_data.data && stream_data.len) {  // avoid nullpo ga!
                        return false;
                    }
                    w.append(stream_data.data, stream_data.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    auto v = Frame::render_len() +
                             streamID.minimum_len() +
                             stream_data.len;
                    if (type.STREAM_off()) {
                        v += offset.minimum_len();
                    }
                    if (type.STREAM_len()) {
                        v += QVarInt{stream_data.len}.minimum_len();
                    }
                    return v;
                }

                constexpr size_t visit_bytelen(auto&& cb) {
                    cb(stream_data);
                    return stream_data.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    cb(stream_data);
                    return stream_data.len;
                }
            };

            struct MaxDataFrame : Frame {
                QVarInt max_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_DATA) {
                        return false;
                    }
                    return max_data.parse(b);
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() + max_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::MAX_DATA) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    return max_data.render(w);
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() + max_data.minimum_len();
                }
            };

            struct MaxStreamDataFrame : Frame {
                QVarInt streamID;
                QVarInt max_stream_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_STREAM_DATA) {
                        return false;
                    }
                    return streamID.parse(b) &&
                           max_stream_data.parse(b);
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() +
                           streamID.len +
                           max_stream_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::MAX_STREAM_DATA) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    return streamID.render(w) && max_stream_data.render(w);
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() +
                           streamID.minimum_len() +
                           max_stream_data.minimum_len();
                }
            };

            struct MaxStreamsFrame : Frame {
                QVarInt max_streams;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_STREAMS) {
                        return false;
                    }
                    return max_streams.parse(b);
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() + max_streams.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::MAX_STREAMS) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    return max_streams.render(w);
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() + max_streams.minimum_len();
                }
            };

            struct DataBlockedFrame : Frame {
                QVarInt max_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::DATA_BLOCKED) {
                        return false;
                    }
                    return max_data.parse(b);
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() + max_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::DATA_BLOCKED) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    return max_data.render(w);
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() + max_data.minimum_len();
                }
            };

            struct StreamDataBlockedFrame : Frame {
                QVarInt streamID;
                QVarInt max_stream_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAM_DATA_BLOCKED) {
                        return false;
                    }
                    return streamID.parse(b) && max_stream_data.parse(b);
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() +
                           streamID.len +
                           max_stream_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::STREAM_DATA_BLOCKED) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    return streamID.render(w) && max_stream_data.render(w);
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() +
                           streamID.minimum_len() +
                           max_stream_data.minimum_len();
                }
            };

            struct StreamsBlockedFrame : Frame {
                QVarInt max_streams;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAMS_BLOCKED) {
                        return false;
                    }
                    return max_streams.parse(b);
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() + max_streams.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::STREAMS_BLOCKED) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    return max_streams.render(w);
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() + max_streams.minimum_len();
                }
            };

            struct NewConnectionIDFrame : Frame {
                QVarInt squecne_number;
                QVarInt retire_proior_to;
                byte length = 0;  // only parse
                ByteLen connectionID;
                ByteLen stateless_reset_token;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::NEW_CONNECTION_ID) {
                        return false;
                    }
                    if (!squecne_number.parse(b) ||
                        !retire_proior_to.parse(b)) {
                        return false;
                    }
                    if (!b.enough(1)) {
                        return false;
                    }
                    length = *b.data;
                    if (!b.fwdresize(connectionID, 1, length)) {
                        return false;
                    }
                    if (!b.fwdresize(stateless_reset_token, length, 16)) {
                        return false;
                    }
                    b = b.forward(16);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() +
                           squecne_number.len +
                           retire_proior_to.len +
                           1 +  // length
                           connectionID.len +
                           stateless_reset_token.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::NEW_CONNECTION_ID) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!squecne_number.render(w) ||
                        !retire_proior_to.render(w)) {
                        return false;
                    }
                    if (connectionID.len > 0xff) {
                        return false;
                    }
                    if (!connectionID.data && connectionID.len) {
                        return false;
                    }
                    w.append(connectionID.data, connectionID.len);
                    if (!stateless_reset_token.data || stateless_reset_token.len != 16) {
                        return false;
                    }
                    w.append(stateless_reset_token.data, 16);
                    return true;
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() +
                           squecne_number.minimum_len() +
                           retire_proior_to.minimum_len() +
                           1 +  // length
                           connectionID.len +
                           16;  // stateless reset token
                }

                constexpr size_t visit_bytelen(auto&& cb) {
                    cb(connectionID);
                    cb(stateless_reset_token);
                    return connectionID.len + stateless_reset_token.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    cb(connectionID);
                    cb(stateless_reset_token);
                    return connectionID.len + stateless_reset_token.len;
                }
            };

            struct RetireConnectionIDFrame : Frame {
                QVarInt sequence_number;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::RETIRE_CONNECTION_ID) {
                        return false;
                    }
                    return sequence_number.parse(b);
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() + sequence_number.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::RETIRE_CONNECTION_ID) {
                        return false;
                    }
                    return sequence_number.render(w);
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() + sequence_number.minimum_len();
                }
            };

            struct PathChallengeFrame : Frame {
                std::uint64_t data = 0;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::PATH_CHALLENGE) {
                        return false;
                    }
                    if (!b.enough(8)) {
                        return false;
                    }
                    data = b.as<std::uint64_t>();
                    b = b.forward(8);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() + 8;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::PATH_CHALLENGE) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    w.as(data);
                    return true;
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() + 8;
                }
            };

            struct PathResponseFrame : Frame {
                std::uint64_t data = 0;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::PATH_RESPONSE) {
                        return false;
                    }
                    if (!b.enough(8)) {
                        return false;
                    }
                    data = b.as<std::uint64_t>();
                    b = b.forward(8);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Frame::parse_len() + 8;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::PATH_RESPONSE) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    w.as(data);
                    return true;
                }

                constexpr size_t render_len() const {
                    return Frame::render_len() + 8;
                }
            };

            struct ConnectionCloseFrame : Frame {
                QVarInt error_code;
                QVarInt frame_type;
                QVarInt reason_phrase_length;  // only parse
                ByteLen reason_phrase;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::CONNECTION_CLOSE) {
                        return false;
                    }
                    if (!error_code.parse(b)) {
                        return false;
                    }
                    if (type.type_detail() != FrameType::CONNECTION_CLOSE_APP) {
                        if (!frame_type.parse(b)) {
                            return false;
                        }
                    }
                    if (!reason_phrase_length.parse(b)) {
                        return false;
                    }
                    if (!b.enough(reason_phrase_length)) {
                        return false;
                    }
                    reason_phrase = b.resized(reason_phrase_length);
                    b = b.forward(reason_phrase_length);
                    return true;
                }

                constexpr size_t parse_len() const {
                    auto v = Frame::parse_len() +
                             error_code.len +
                             reason_phrase_length.len +
                             reason_phrase.len;
                    if (type.type_detail() != FrameType::CONNECTION_CLOSE_APP) {
                        v += frame_type.len;
                    }
                    return v;
                }

                constexpr bool render(WPacket& w) const {
                    if (type.type() != FrameType::CONNECTION_CLOSE) {
                        return false;
                    }
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!error_code.render(w)) {
                        return false;
                    }
                    if (type.type_detail() != FrameType::CONNECTION_CLOSE_APP) {
                        if (!frame_type.render(w)) {
                            return false;
                        }
                    }
                    if (!QVarInt{reason_phrase.len}.render(w)) {
                        return false;
                    }
                    if (!reason_phrase.data && reason_phrase.len) {
                        return false;
                    }
                    w.append(reason_phrase.data, reason_phrase.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    auto v = Frame::render_len() +
                             error_code.minimum_len() +
                             QVarInt{reason_phrase.len}.minimum_len() +
                             reason_phrase.len;
                    if (type.type_detail() != FrameType::CONNECTION_CLOSE_APP) {
                        v += frame_type.minimum_len();
                    }
                    return v;
                }

                constexpr size_t visit_bytelen(auto&& cb) {
                    cb(reason_phrase);
                    return reason_phrase.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    cb(reason_phrase);
                    return reason_phrase.len;
                }
            };

            struct HandshakeDoneFrame : Frame {
                constexpr bool parse(ByteLen& b) {
                    return Frame::parse(b) &&
                           type.type() == FrameType::HANDSHAKE_DONE;
                }

                constexpr size_t parse_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    return type.type() == FrameType::HANDSHAKE_DONE &&
                           Frame::render(w);
                }

                constexpr size_t render_len() const {
                    return 1;
                }
            };

            struct DatagramFrame : Frame {
                QVarInt length;  // only parse
                ByteLen datagram_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::DATAGRAM) {
                        return false;
                    }
                    if (type.DATAGRAM_len()) {
                        if (!length.parse(b)) {
                            return false;
                        }
                        if (!b.enough(length)) {
                            return false;
                        }
                        datagram_data = b.resized(length);
                        b = b.forward(length);
                    }
                    else {
                        datagram_data = b;
                        b = b.forward(b.len);
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    auto v = Frame::parse_len() +
                             datagram_data.len;
                    if (type.DATAGRAM_len()) {
                        v += length.len;
                    }
                    return v;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::DATAGRAM) {
                        return false;
                    }
                    if (type.DATAGRAM_len()) {
                        if (!QVarInt{datagram_data.len}.render(w)) {
                            return false;
                        }
                    }
                    if (!datagram_data.data && datagram_data.len) {
                        return false;
                    }
                    w.append(datagram_data.data, datagram_data.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    auto v = Frame::render_len() +
                             datagram_data.len;
                    if (type.DATAGRAM_len()) {
                        v += QVarInt{datagram_data.len}.minimum_len();
                    }
                    return v;
                }

                constexpr size_t visit_bytelen(auto&& cb) {
                    cb(datagram_data);
                    return datagram_data.len;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    cb(datagram_data);
                    return datagram_data.len;
                }
            };

        }  // namespace quic::frame
    }      // namespace dnet
}  // namespace utils

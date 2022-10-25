/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame - QUIC frame format
#pragma once
#include "../bytelen.h"

namespace utils {
    namespace dnet {
        namespace quic {

            struct Frame {
                FrameFlags type;

                constexpr bool parse(ByteLen& b) {
                    if (!b.enough(1)) {
                        return false;
                    }
                    type = {b.data};
                    b = b.forward(1);
                    return true;
                }

                constexpr size_t frame_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    if (!type.value) {
                        return false;
                    }
                    w.append(type.value, 1);
                    return true;
                }
            };

            struct PaddingFrame : Frame {
                constexpr bool parse(ByteLen& b) {
                    return Frame::parse(b) &&
                           type.type() == FrameType::PADDING;
                }

                constexpr size_t frame_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    return type.type() == FrameType::PADDING &&
                           Frame::render(w);
                }
            };

            struct PingFrame : Frame {
                constexpr bool parse(ByteLen& b) {
                    return Frame::parse(b) &&
                           type.type() == FrameType::PING;
                }

                constexpr size_t frame_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    return type.type() == FrameType::PING &&
                           Frame::render(w);
                }
            };

            struct ACKFrame : Frame {
                ByteLen largest_ack;
                ByteLen ack_delay;
                ByteLen ack_range_count;
                ByteLen first_ack_range;
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
                    if (!b.qvarintfwd(largest_ack)) {
                        return false;
                    }
                    if (!b.qvarintfwd(ack_delay)) {
                        return false;
                    }
                    if (!b.qvarintfwd(ack_range_count)) {
                        return false;
                    }
                    if (!b.qvarintfwd(first_ack_range)) {
                        return false;
                    }
                    auto count = ack_range_count.qvarint();
                    auto len = parse_ack_range(b, count, [](auto...) {});
                    if (count != 0 && !len) {
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

                constexpr size_t frmae_len() const {
                    auto v = Frame::frame_len() +
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
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::ACK) {
                        return false;
                    }
                    if (!largest_ack.is_qvarint_valid() ||
                        !ack_delay.is_qvarint_valid() ||
                        !ack_ranges.is_qvarint_valid() ||
                        !first_ack_range.is_qvarint_valid()) {
                        return false;
                    }
                    auto count = ack_range_count.qvarint();
                    auto range_len = parse_ack_range(ack_ranges, count, [](auto...) {});
                    if (count != 0 && !range_len) {
                        return false;
                    }
                    w.append(largest_ack.data, largest_ack.qvarintlen());
                    w.append(ack_delay.data, ack_delay.qvarintlen());
                    w.append(ack_range_count.data, ack_range_count.qvarintlen());
                    w.append(first_ack_range.data, first_ack_range.qvarintlen());
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
            };

            namespace ack {

                struct ACKRange {
                    size_t smallest = 0;
                    size_t largest = 0;
                };

                constexpr bool validate_ack_ranges(auto& vec) {
                    for (ACKRange r : vec) {
                        if (r.smallest > r.largest) {
                            return false;
                        }
                    }
                    for (size_t i = 0; i < vec.size(); i++) {
                        if (i == 0) {
                            continue;
                        }
                        ACKRange r = vec[i];
                        ACKRange last = vec[i - 1];
                        if (last.smallest <= r.smallest || last.largest <= r.largest) {
                            return false;
                        }
                    }
                    return true;
                }

                constexpr bool get_ackranges(auto& ranges, const ACKFrame& ack) {
                    auto largest_ack = ack.largest_ack.qvarint();
                    auto ackBlock = ack.first_ack_range.qvarint();
                    auto smallest = largest_ack - ackBlock;
                    ranges.push_back(ACKRange{smallest, largest_ack});
                    auto res = ACKFrame::parse_ack_range(ack.ack_ranges, ack.ack_range_count.qvarint(), [&](auto gap, auto ackBlock) {
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
                        return false;
                    }
                    if (!validate_ack_ranges(ranges)) {
                        return false;
                    }
                    return true;
                }
            }  // namespace ack

            struct ResetStreamFrame : Frame {
                ByteLen streamID;
                ByteLen application_protocol_error_code;
                ByteLen fianl_size;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::RESET_STREAM) {
                        return false;
                    }
                    if (!b.qvarintfwd(streamID)) {
                        return false;
                    }
                    if (!b.qvarintfwd(application_protocol_error_code)) {
                        return false;
                    }
                    if (!b.qvarintfwd(fianl_size)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() +
                           streamID.len +
                           application_protocol_error_code.len +
                           fianl_size.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::RESET_STREAM) {
                        return false;
                    }
                    if (!streamID.is_qvarint_valid() ||
                        !application_protocol_error_code.is_qvarint_valid() ||
                        !fianl_size.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(streamID.data, streamID.qvarintlen());
                    w.append(application_protocol_error_code.data, application_protocol_error_code.qvarintlen());
                    w.append(fianl_size.data, fianl_size.qvarintlen());
                    return true;
                }
            };

            struct StopSendingFrame : Frame {
                ByteLen streamID;
                ByteLen application_protocol_error_code;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STOP_SENDING) {
                        return false;
                    }
                    if (!b.qvarintfwd(streamID)) {
                        return false;
                    }
                    if (!b.qvarintfwd(application_protocol_error_code)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() +
                           streamID.len +
                           application_protocol_error_code.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::STOP_SENDING) {
                        return false;
                    }
                    if (!streamID.is_qvarint_valid() ||
                        !application_protocol_error_code.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(streamID.data, streamID.qvarintlen());
                    w.append(application_protocol_error_code.data, application_protocol_error_code.qvarintlen());
                    return true;
                }
            };

            struct CryptoFrame : Frame {
                ByteLen offset;
                ByteLen length;
                ByteLen crypto_data;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::CRYPTO) {
                        return false;
                    }
                    if (!b.qvarintfwd(offset)) {
                        return false;
                    }
                    if (!b.qvarintfwd(length)) {
                        return false;
                    }
                    auto clen = length.qvarint();
                    if (!b.enough(clen)) {
                        return false;
                    }
                    crypto_data = b.resized(clen);
                    b = b.forward(clen);
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() +
                           offset.len +
                           length.len +
                           crypto_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::CRYPTO) {
                        return false;
                    }
                    if (!offset.is_qvarint_valid() ||
                        !length.is_qvarint_valid()) {
                        return false;
                    }
                    auto clen = length.qvarint();
                    if (!crypto_data.enough(clen)) {
                        return false;
                    }
                    w.append(offset.data, offset.qvarintlen());
                    w.append(length.data, length.qvarintlen());
                    w.append(crypto_data.data, clen);
                    return true;
                }
            };

            struct NewTokenFrame : Frame {
                ByteLen token_length;
                ByteLen token;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::NEW_TOKEN) {
                        return false;
                    }
                    if (!b.qvarintfwd(token_length)) {
                        return false;
                    }
                    auto tlen = token_length.qvarint();
                    if (!b.enough(tlen)) {
                        return false;
                    }
                    token = b.resized(tlen);
                    b = b.forward(tlen);
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() +
                           token_length.len +
                           token.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::NEW_TOKEN) {
                        return false;
                    }
                    if (!token_length.is_qvarint_valid()) {
                        return false;
                    }
                    auto tlen = token_length.qvarint();
                    if (!token.enough(tlen)) {
                        return false;
                    }
                    w.append(token_length.data, token_length.qvarint());
                    w.append(token.data, tlen);
                    return true;
                }
            };

            struct StreamFrame : Frame {
                ByteLen streamID;
                ByteLen offset;
                ByteLen length;
                ByteLen stream_data;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAM) {
                        return false;
                    }
                    if (!b.qvarintfwd(streamID)) {
                        return false;
                    }
                    if (type.STREAM_off()) {
                        if (!b.qvarintfwd(offset)) {
                            return false;
                        }
                    }
                    if (type.STREAM_len()) {
                        if (!b.qvarintfwd(length)) {
                            return false;
                        }
                        auto len = length.qvarint();
                        if (!b.enough(len)) {
                            return false;
                        }
                        stream_data = b.resized(len);
                        b = b.forward(len);
                    }
                    else {
                        stream_data = b;
                        b = b.forward(b.len);
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    auto v = Frame::frame_len() +
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
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAM) {
                        return false;
                    }
                    if (!streamID.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(streamID.data, streamID.qvarintlen());
                    if (type.STREAM_off()) {
                        if (!offset.is_qvarint_valid()) {
                            return false;
                        }
                        w.append(offset.data, offset.qvarintlen());
                    }
                    if (type.STREAM_len()) {
                        if (!length.is_qvarint_valid()) {
                            return false;
                        }
                        w.append(length.data, length.qvarintlen());
                        auto len = length.qvarint();
                        if (!stream_data.enough(len)) {
                            return false;
                        }
                        w.append(stream_data.data, len);
                    }
                    else {
                        if (!stream_data.valid()) {
                            return false;
                        }
                        w.append(stream_data.data, stream_data.len);
                    }
                    return true;
                }
            };

            struct MaxDataFrame : Frame {
                ByteLen max_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_DATA) {
                        return false;
                    }
                    if (!b.qvarintfwd(max_data)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() + max_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_DATA) {
                        return false;
                    }
                    if (!max_data.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(max_data.data, max_data.qvarintlen());
                    return true;
                }
            };

            struct MaxStreamDataFrame : Frame {
                ByteLen streamID;
                ByteLen max_stream_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_STREAM_DATA) {
                        return false;
                    }
                    if (!b.qvarintfwd(streamID)) {
                        return false;
                    }
                    if (!b.qvarintfwd(max_stream_data)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() +
                           streamID.len +
                           max_stream_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_STREAM_DATA) {
                        return false;
                    }
                    if (!streamID.is_qvarint_valid() ||
                        !max_stream_data.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(streamID.data, streamID.qvarintlen());
                    w.append(max_stream_data.data, max_stream_data.qvarintlen());
                    return true;
                }
            };

            struct MaxStreamsFrame : Frame {
                ByteLen max_streams;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_STREAMS) {
                        return false;
                    }
                    if (!b.qvarintfwd(max_streams)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() + max_streams.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::MAX_STREAMS) {
                        return false;
                    }
                    if (!max_streams.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(max_streams.data, max_streams.qvarintlen());
                    return true;
                }
            };

            struct DataBlockedFrame : Frame {
                ByteLen max_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::DATA_BLOCKED) {
                        return false;
                    }
                    if (!b.qvarintfwd(max_data)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() + max_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::DATA_BLOCKED) {
                        return false;
                    }
                    if (!max_data.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(max_data.data, max_data.qvarintlen());
                    return true;
                }
            };

            struct StreamDataBlockedFrame : Frame {
                ByteLen streamID;
                ByteLen max_stream_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAM_DATA_BLOCKED) {
                        return false;
                    }
                    if (!b.qvarintfwd(streamID)) {
                        return false;
                    }
                    if (!b.qvarintfwd(max_stream_data)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() +
                           streamID.len +
                           max_stream_data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAM_DATA_BLOCKED) {
                        return false;
                    }
                    if (!streamID.is_qvarint_valid() ||
                        !max_stream_data.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(streamID.data, streamID.qvarintlen());
                    w.append(max_stream_data.data, max_stream_data.qvarintlen());
                    return true;
                }
            };

            struct StreamsBlockedFrame : Frame {
                ByteLen max_streams;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAMS_BLOCKED) {
                        return false;
                    }
                    if (!b.qvarintfwd(max_streams)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() + max_streams.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::STREAMS_BLOCKED) {
                        return false;
                    }
                    if (!max_streams.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(max_streams.data, max_streams.qvarintlen());
                    return true;
                }
            };

            struct NewConnectionIDFrame : Frame {
                ByteLen squecne_number;
                ByteLen retire_proior_to;
                byte* length;
                ByteLen connectionID;
                ByteLen stateless_reset_token;
                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::NEW_CONNECTION_ID) {
                        return false;
                    }
                    if (!b.qvarintfwd(squecne_number)) {
                        return false;
                    }
                    if (!b.qvarintfwd(retire_proior_to)) {
                        return false;
                    }
                    if (!b.enough(1)) {
                        return false;
                    }
                    length = b.data;
                    if (!b.fwdresize(connectionID, 1, *length)) {
                        return false;
                    }
                    if (!b.fwdresize(stateless_reset_token, *length, 16)) {
                        return false;
                    }
                    b = b.forward(16);
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() +
                           squecne_number.len +
                           retire_proior_to.len +
                           1 +  // length
                           connectionID.len +
                           stateless_reset_token.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::NEW_CONNECTION_ID) {
                        return false;
                    }
                    if (!squecne_number.is_qvarint_valid() ||
                        !retire_proior_to.is_qvarint_valid() ||
                        !length ||
                        !connectionID.enough(*length) ||
                        !stateless_reset_token.enough(16)) {
                        return false;
                    }
                    w.append(squecne_number.data, squecne_number.qvarintlen());
                    w.append(retire_proior_to.data, retire_proior_to.qvarintlen());
                    w.append(length, 1);
                    w.append(connectionID.data, *length);
                    w.append(stateless_reset_token.data, 16);
                    return true;
                }
            };

            struct RetireConnectionIDFrame : Frame {
                ByteLen sequence_number;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::RETIRE_CONNECTION_ID) {
                        return false;
                    }
                    if (!b.qvarintfwd(sequence_number)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() + sequence_number.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::RETIRE_CONNECTION_ID) {
                        return false;
                    }
                    if (!sequence_number.is_qvarint_valid()) {
                        return false;
                    }
                    w.append(sequence_number.data, sequence_number.qvarintlen());
                    return true;
                }
            };

            struct PathChallengeFrame : Frame {
                ByteLen data;

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
                    data = b.resized(8);
                    b = b.forward(8);
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() + data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::PATH_CHALLENGE) {
                        return false;
                    }
                    if (!data.enough(8)) {
                        return false;
                    }
                    w.append(data.data, 8);
                    return true;
                }
            };

            struct PathResponseFrame : Frame {
                ByteLen data;

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
                    data = b.resized(8);
                    b = b.forward(8);
                    return true;
                }

                constexpr size_t frame_len() const {
                    return Frame::frame_len() + data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (type.type() != FrameType::PATH_RESPONSE) {
                        return false;
                    }
                    if (!data.enough(8)) {
                        return false;
                    }
                    w.append(data.data, 8);
                    return true;
                }
            };

            struct ConnectionCloseFrame : Frame {
                ByteLen error_code;
                ByteLen frame_type;
                ByteLen reason_phrase_length;
                ByteLen reason_phrase;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::CONNECTION_CLOSE) {
                        return false;
                    }
                    if (!b.qvarintfwd(error_code)) {
                        return false;
                    }
                    if (type.type_detail() != FrameType::CONNECTION_CLOSE_APP) {
                        if (!b.qvarintfwd(frame_type)) {
                            return false;
                        }
                    }
                    if (b.qvarintfwd(reason_phrase_length)) {
                        return false;
                    }
                    auto len = reason_phrase_length.qvarint();
                    if (!b.enough(len)) {
                        return false;
                    }
                    reason_phrase = b.resized(len);
                    b = b.forward(len);
                    return true;
                }

                constexpr size_t frame_len() const {
                    auto v = Frame::frame_len() +
                             error_code.len +
                             reason_phrase_length.len +
                             reason_phrase.len;
                    if (type.type_detail() != FrameType::CONNECTION_CLOSE_APP) {
                        v += frame_type.len;
                    }
                    return v;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Frame::render(w)) {
                        return false;
                    }
                    if (!error_code.is_qvarint_valid() ||
                        !reason_phrase_length.is_qvarint_valid()) {
                        return false;
                    }
                    auto rlen = reason_phrase_length.qvarint();
                    if (!reason_phrase.enough(rlen)) {
                        return false;
                    }
                    w.append(error_code.data, error_code.qvarintlen());
                    if (type.type_detail() != FrameType::CONNECTION_CLOSE_APP) {
                        if (!frame_type.is_qvarint_valid()) {
                            return false;
                        }
                        w.append(frame_type.data, frame_type.qvarintlen());
                    }
                    w.append(reason_phrase_length.data, reason_phrase_length.qvarintlen());
                    w.append(reason_phrase.data, rlen);
                    return true;
                }
            };

            struct HandshakeDoneFrame : Frame {
                constexpr bool parse(ByteLen& b) {
                    return Frame::parse(b) &&
                           type.type() == FrameType::HANDSHAKE_DONE;
                }

                constexpr size_t frame_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    return type.type() == FrameType::HANDSHAKE_DONE &&
                           Frame::render(w);
                }
            };

            struct DatagramFrame : Frame {
                ByteLen length;
                ByteLen datagram_data;

                constexpr bool parse(ByteLen& b) {
                    if (!Frame::parse(b)) {
                        return false;
                    }
                    if (type.type() != FrameType::DATAGRAM) {
                        return false;
                    }
                    if (type.DATAGRAM_len()) {
                        if (!b.qvarintfwd(length)) {
                            return false;
                        }
                        auto len = length.qvarint();
                        if (!b.enough(len)) {
                            return false;
                        }
                        datagram_data = b.resized(len);
                        b = b.forward(len);
                    }
                    else {
                        datagram_data = b;
                        b = b.forward(b.len);
                    }
                    return true;
                }

                constexpr size_t frame_len() const {
                    auto v = Frame::frame_len() +
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
                        if (!length.is_qvarint_valid()) {
                            return false;
                        }
                        w.append(length.data, length.qvarintlen());
                        auto len = length.qvarint();
                        if (!datagram_data.enough(len)) {
                            return false;
                        }
                        w.append(datagram_data.data, len);
                    }
                    else {
                        if (!datagram_data.valid()) {
                            return false;
                        }
                        w.append(datagram_data.data, datagram_data.len);
                    }
                    return true;
                }
            };

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

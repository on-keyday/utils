/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../frame/cast.h"
#include "format.h"

namespace futils::fnet::quic::log {
    // if omit_data is true, data field (crypto/stream/connection_close(reason_phrase)/datagram) will be ommited
    // if data_as_hex is true, print above data as hexadecimal format, otherwise print like string
    // (like "\x02h3")
    template <template <class...> class Vec>
    constexpr void format_frame(auto&& out, const auto& frame, bool omit_data, bool data_as_hex) {
        const auto numfield = fmt_field(out, fmt_int());
        const auto typefield = fmt_frame_type();
        const auto hexfield = fmt_field(out, fmt_hex());
        const auto group = fmt_object_or_array(out);
        const auto stringfield = fmt_field(out, fmt_string());
        const auto nullfield = [](auto&&...) {};
        auto do_format = [&](const auto& frame, auto& datafield) {
#define SWITCH()           \
    if constexpr (false) { \
    }
#define CASE(F) \
    else if constexpr (std::is_same_v<decltype(frame), const F&>)
            typefield(out, frame.type);
            group(nullptr, false, [&] {
                SWITCH()
                CASE(frame::ACKFrame<Vec>) {
                    const frame::ACKFrame<Vec>& f = frame;
                    numfield("largest_ack", f.largest_ack);
                    numfield("ack_delay", f.ack_delay);
                    numfield("ack_range_count", f.ack_range_count);
                    numfield("first_ack_range", f.first_ack_range);
                    group("ack_ranges", true, [&] {
                        for (const frame::WireACKRange& val : f.ack_ranges) {
                            group(nullptr, false, [&] {
                                numfield("gap", val.gap);
                                numfield("len", val.len);
                            });
                        }
                    });
                    if (f.type.type_detail() == FrameType::ACK_ECN) {
                        group("ecn_counts", false, [&] {
                            numfield("ect0", f.ecn_counts.ect0);
                            numfield("ect1", f.ecn_counts.ect1);
                            numfield("ecn_ce", f.ecn_counts.ecn_ce);
                        });
                    }
                }
                CASE(frame::ResetStreamFrame) {
                    const frame::ResetStreamFrame& f = frame;
                    numfield("stream_id", f.streamID);
                    numfield("application_error_code", f.application_protocol_error_code);
                    numfield("final_size", f.final_size);
                }
                CASE(frame::StopSendingFrame) {
                    const frame::StopSendingFrame& f = frame;
                    numfield("stream_id", f.streamID);
                    numfield("application_error_code", f.application_protocol_error_code);
                }
                CASE(frame::CryptoFrame) {
                    const frame::CryptoFrame& f = frame;
                    numfield("offset", f.offset);
                    numfield("length", f.length);
                    numfield("offset+length", f.offset + f.length);
                    datafield("cryoto_data", f.crypto_data);
                }
                CASE(frame::NewTokenFrame) {
                    const frame::NewTokenFrame& f = frame;
                    numfield("token_length", f.token_length);
                    hexfield("token", f.token);
                }
                CASE(frame::StreamFrame) {
                    const frame::StreamFrame& f = frame;
                    numfield("stream_id", f.streamID);
                    if (f.type.STREAM_off()) {
                        numfield("offset", f.offset);
                    }
                    if (f.type.STREAM_len()) {
                        numfield("length", f.length);
                    }
                    numfield("offset+length", f.offset + f.stream_data.size());
                    datafield("stream_data", f.stream_data);
                }
                CASE(frame::MaxDataFrame) {
                    const frame::MaxDataFrame& f = frame;
                    numfield("max_data", f.max_data);
                }
                CASE(frame::MaxStreamDataFrame) {
                    const frame::MaxStreamDataFrame& f = frame;
                    numfield("stream_id", f.streamID);
                    numfield("max_stream_data", f.max_stream_data);
                }
                CASE(frame::MaxStreamsFrame) {
                    const frame::MaxStreamsFrame& f = frame;
                    numfield("max_streams", f.max_streams);
                }
                CASE(frame::DataBlockedFrame) {
                    const frame::DataBlockedFrame& f = frame;
                    numfield("max_data", f.max_data, 10);
                }
                CASE(frame::StreamDataBlockedFrame) {
                    const frame::StreamDataBlockedFrame& f = frame;
                    numfield("stream_id", f.streamID);
                    numfield("max_stream_data", f.max_stream_data);
                }
                CASE(frame::StreamsBlockedFrame) {
                    const frame::StreamsBlockedFrame& f = frame;
                    numfield("max_streams", f.max_streams);
                }
                CASE(frame::NewConnectionIDFrame) {
                    const frame::NewConnectionIDFrame& f = frame;
                    numfield("sequence_number", f.sequence_number);
                    numfield("retire_proior_to", f.retire_proior_to);
                    numfield("length", f.length);
                    hexfield("connection_id", f.connectionID);
                    hexfield("stateless_reset_token", f.stateless_reset_token);
                }
                CASE(frame::RetireConnectionIDFrame) {
                    const frame::RetireConnectionIDFrame& f = frame;
                    numfield("sequence_number", f.sequence_number);
                }
                CASE(frame::PathChallengeFrame) {
                    const frame::PathChallengeFrame& f = frame;
                    numfield("data", f.data, 16);
                }
                CASE(frame::PathResponseFrame) {
                    const frame::PathResponseFrame& f = frame;
                    numfield("data", f.data, 16);
                }
                CASE(frame::ConnectionCloseFrame) {
                    const frame::ConnectionCloseFrame& f = frame;
                    numfield("error_code", f.error_code);
                    if (f.type.type_detail() == FrameType::CONNECTION_CLOSE) {
                        fmt_field(out, fmt_frame_type())("frame_type", f.frame_type.value);
                    }
                    numfield("reason_phrase_length", f.reason_phrase_length);
                    datafield("reason_phrase", f.reason_phrase);
                }
                CASE(frame::DatagramFrame) {
                    const frame::DatagramFrame& f = frame;
                    numfield("length", f.length);
                    datafield("datagram_data", f.datagram_data);
                }
            });
#undef SWITCH
#undef CASE
        };
        auto call_format = [&](const auto& frame) {
            if (omit_data) {
                do_format(frame, nullfield);
            }
            else if (data_as_hex) {
                do_format(frame, hexfield);
            }
            else {
                do_format(frame, stringfield);
            }
            erase_comma_if_exist(out);
        };
        if constexpr (std::is_same_v<decltype(frame), const frame::Frame&>) {
            frame::select_Frame<Vec>(frame.type.type(), [&](const auto& f) {
                call_format(static_cast<decltype(f)>(frame));
            });
        }
        else {
            call_format(frame);
        }
    }
}  // namespace futils::fnet::quic::log

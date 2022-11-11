/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// eframe - easy mapped frame
#pragma once
#include "frame.h"
#include "frame_make.h"
#include "frame_util.h"

namespace utils {
    namespace dnet {
        namespace quic::frame {

            template <class T, FrameType type>
            struct OneByte {
                using byte_frame = T;
                static constexpr auto frame_type = type;

                constexpr bool from(const T& p) {
                    return p.type.type() == type;
                }

                constexpr bool to(WPacket& src, T& p) const {
                    p.type = src.frame_type(type);
                    return p.type.valid();
                }
            };

            struct Padding : OneByte<PaddingFrame, FrameType::PADDING> {};
            struct Ping : OneByte<PingFrame, FrameType::PING> {};

            constexpr bool render_ok(const auto& r) {
                WPacket w;
                return r.render(w);
            }

            template <template <class...> class Vec>
            struct ACK {
                using byte_frame = ACKFrame;
                static constexpr auto frame_type = FrameType::ACK;
                size_t ack_delay = 0;
                Vec<ACKRange> ack_ranges;
                bool has_ecn = false;
                ECNCounts ecn;

                constexpr bool from(const ACKFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    ack_delay = f.ack_delay.qvarint();
                    if (!get_ackranges(ack_ranges, f)) {
                        return false;
                    }
                    if (f.type.type_detail() == FrameType::ACK_ECN) {
                        has_ecn = true;
                        if (!f.parse_ecn_counts(f.ecn_counts, [&](auto e0, auto e1, auto ce) {
                                ecn.ect0 = e0;
                                ecn.ect1 = e1;
                                ecn.ecn_ce = ce;
                            })) {
                            return false;
                        }
                    }
                    return true;
                }

                constexpr bool to(WPacket& w, ACKFrame& f) const {
                    f = make_ack(w, ack_delay, ack_ranges, has_ecn ? &ecn : nullptr);
                    return render_ok(f);
                }
            };

            struct ResetStream {
                size_t stream_id = 0;
                size_t error_code = 0;
                size_t final_size = 0;
                constexpr bool from(const ResetStreamFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    stream_id = f.streamID.qvarint();
                    error_code = f.application_protocol_error_code.qvarint();
                    final_size = f.fianl_size.qvarint();
                    return true;
                }

                constexpr bool to(WPacket& w, ResetStreamFrame& f) const {
                    f = make_reset_stream(w, stream_id, error_code, final_size);
                    return render_ok(f);
                }
            };

            struct StopSending {
                size_t stream_id = 0;
                size_t error_code = 0;

                constexpr bool from(const StopSendingFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    stream_id = f.streamID.qvarint();
                    error_code = f.application_protocol_error_code.qvarint();
                    return true;
                }

                constexpr bool to(WPacket& w, StopSendingFrame& f) const {
                    f = make_stop_sending(w, stream_id, error_code);
                    return render_ok(f);
                }
            };

            struct Crypto {
                size_t offset = 0;
                ByteLen data;

                constexpr bool from(const CryptoFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    offset = f.offset.qvarint();
                    data = f.crypto_data.resized(f.length.qvarint());
                    return true;
                }

                constexpr bool to(WPacket& w, CryptoFrame& f, bool copy = true) const {
                    f = make_crypto(w, offset, data, copy);
                    return render_ok(f);
                }
            };

            struct NewToken {
                ByteLen token;

                constexpr bool from(const NewTokenFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    token = f.token.resized(f.token_length.qvarint());
                    return true;
                }

                constexpr bool to(WPacket& w, NewTokenFrame& f, bool copy = true) const {
                    f = make_new_token(w, token, copy);
                    return render_ok(f);
                }
            };

            struct Stream {
                size_t stream_id = 0;
                size_t offset = ~0;
                bool len = false;
                bool fin = false;
                ByteLen data;

                constexpr bool from(const StreamFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    if (f.type.STREAM_len()) {
                        data = f.stream_data.resized(f.length.qvarint());
                        len = true;
                    }
                    else {
                        data = f.stream_data;
                        len = false;
                    }
                    if (f.type.STREAM_off()) {
                        offset = f.offset.qvarint();
                    }
                    fin = f.type.STREAM_fin();
                    return true;
                }

                constexpr bool to(WPacket& src, StreamFrame& f, bool copy = true) const {
                    f = make_stream(src, stream_id, data, len, fin, offset, copy);
                    return render_ok(f);
                }
            };

            struct MaxData {
                size_t max_data = 0;
                constexpr bool from(const MaxDataFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    max_data = f.max_data.qvarint();
                    return true;
                }

                constexpr bool to(WPacket& src, MaxDataFrame& f) const {
                    f = make_max_data(src, max_data);
                    return render_ok(f);
                }
            };

            struct MaxStreamData {
                size_t stream_id = 0;
                size_t max_data = 0;

                constexpr bool from(const MaxStreamDataFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    stream_id = f.streamID.qvarint();
                    max_data = f.max_stream_data.qvarint();
                }

                constexpr bool to(WPacket& src, MaxStreamDataFrame& f) const {
                    f = make_max_stream_data(src, stream_id, max_data);
                    return render_ok(f);
                }
            };

            struct MaxStreams {
                size_t max_streams = 0;
                bool uni = false;
                constexpr bool from(const MaxStreamsFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    max_streams = f.max_streams.qvarint();
                    uni = f.type.type_detail() == FrameType::MAX_STREAMS_UNI;
                }

                constexpr bool to(WPacket& src, MaxStreamsFrame& f) const {
                    f = make_max_streams(src, uni, max_streams);
                    return render_ok(f);
                }
            };

            struct DataBlocked {
                size_t max_data = 0;
                constexpr bool from(const DataBlockedFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    max_data = f.max_data.qvarint();
                    return true;
                }

                constexpr bool to(WPacket& src, DataBlockedFrame& f) const {
                    f = make_data_blocked(src, max_data);
                    return render_ok(f);
                }
            };

            struct StreamDataBlocked {
                size_t stream_id = 0;
                size_t max_data = 0;

                constexpr bool from(const StreamDataBlockedFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    stream_id = f.streamID.qvarint();
                    max_data = f.max_stream_data.qvarint();
                }

                constexpr bool to(WPacket& src, StreamDataBlockedFrame& f) const {
                    f = make_stream_data_blocked(src, stream_id, max_data);
                    return render_ok(f);
                }
            };

            struct StreamsBlocked {
                size_t max_streams = 0;
                bool uni = false;
                constexpr bool from(const StreamsBlockedFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    max_streams = f.max_streams.qvarint();
                    uni = f.type.type_detail() == FrameType::MAX_STREAMS_UNI;
                }

                constexpr bool to(WPacket& src, StreamsBlockedFrame& f) const {
                    f = make_streams_blocked(src, uni, max_streams);
                    return render_ok(f);
                }
            };

            struct NewConnectionID {
                size_t seq_number = 0;
                size_t retire_proior_to = 0;
                ByteLen connID;
                ByteLen stateless_reset_token;

                constexpr bool from(const NewConnectionIDFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    seq_number = f.squecne_number.qvarint();
                    retire_proior_to = f.retire_proior_to.qvarint();
                    connID = f.connectionID.resized(*f.length);
                    stateless_reset_token = f.stateless_reset_token.resized(16);
                    return true;
                }

                constexpr bool to(WPacket& src, NewConnectionIDFrame& f, bool copy = true) {
                    f = make_new_connection_id(src, seq_number, retire_proior_to, connID, stateless_reset_token, copy);
                    return render_ok(f);
                }
            };

            struct RetireConnectionID {
                size_t seq_number = 0;
                constexpr bool from(const RetireConnectionIDFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    seq_number = f.sequence_number.qvarint();
                    return true;
                }

                constexpr bool to(WPacket& src, RetireConnectionIDFrame& f) const {
                    f = make_retire_connection_id(src, seq_number);
                    return render_ok(f);
                }
            };

            struct PathChallenge {
                size_t data = 0;

                constexpr bool from(const PathChallengeFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    data = f.data.as<std::uint64_t>();
                    return true;
                }

                constexpr bool to(WPacket& src, PathChallengeFrame& f) const {
                    f = make_path_challange(src, data);
                    return render_ok(f);
                }
            };

            struct PathResponse {
                size_t data = 0;

                constexpr bool from(const PathResponseFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    data = f.data.as<std::uint64_t>();
                    return true;
                }

                constexpr bool to(WPacket& src, PathResponseFrame& f) const {
                    f = make_path_response(src, data);
                    return render_ok(f);
                }
            };

            struct ConnectionClose {
                bool app = false;
                size_t errcode = 0;
                FrameType type = FrameType::PADDING;
                ConstByteLen reason;
                constexpr bool from(const ConnectionCloseFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    errcode = f.error_code.qvarint();
                    app = f.type.type_detail() == FrameType::CONNECTION_CLOSE_APP;
                    if (!app) {
                        type = FrameType(f.frame_type.qvarint());
                    }
                    auto rp = f.reason_phrase.resized(f.reason_phrase_length.qvarint());
                    reason.data = rp.data;
                    reason.len = rp.len;
                    return true;
                }

                constexpr bool to(WPacket& src, ConnectionCloseFrame& f) const {
                    f = make_connection_close(src, type, app, errcode, reason);
                    return render_ok(f);
                }
            };

            struct HandshakeDone : OneByte<HandshakeDoneFrame, FrameType::HANDSHAKE_DONE> {};

            struct Datagram {
                bool len = false;
                ByteLen data;

                constexpr bool from(const DatagramFrame& f) {
                    if (!render_ok(f)) {
                        return false;
                    }
                    if (f.type.DATAGRAM_len()) {
                        data = f.datagram_data.resized(f.length.qvarint());
                        len = true;
                    }
                    else {
                        data = f.datagram_data;
                        len = false;
                    }
                    return true;
                }

                constexpr bool to(WPacket& src, DatagramFrame& f, bool copy = true) {
                    f = make_datagrm(src, data, len, copy);
                    return render_ok(f);
                }
            };

            template <template <class...> class Vec>
            bool bframe_to_eframe(auto&& f, auto&& cb) {
                using T = std::decay_t<decltype(f)>;
                using ACK = frame::ACK<Vec>;
                auto invoke_cb = [&](auto&& ef) {
                    if (internal::has_logical_not<decltype(cb(ef))>) {
                        if (!cb(ef)) {
                            return false;
                        }
                    }
                    else {
                        cb(ef);
                    }
                    return true;
                };
#define PARSE(TYPE)                                 \
    if constexpr (std::is_same_v<T, TYPE##Frame>) { \
        return invoke_cb(TYPE{});                   \
    }
                PARSE(Padding)
                PARSE(Ping)
                PARSE(ACK)
                PARSE(ResetStream)
                PARSE(StopSending)
                PARSE(Crypto)
                PARSE(NewToken)
                PARSE(Stream)
                PARSE(MaxData)
                PARSE(MaxStreamData)
                PARSE(MaxStreams)
                PARSE(DataBlocked)
                PARSE(StreamDataBlocked)
                PARSE(StreamsBlocked)
                PARSE(NewConnectionID)
                PARSE(RetireConnectionID)
                PARSE(PathChallenge)
                PARSE(PathResponseFrame)
                PARSE(ConnectionClose)
                PARSE(HandshakeDone)
                PARSE(Datagram)
                return false;
#undef PARSE
            }

            template <template <class...> class Vec>
            bool frame_type_to_EFrame(FrameType f, auto&& cb) {
                return frame_type_to_Type(f, [&](auto&& bf) {
                    return bframe_to_eframe<Vec>(bf, [&](auto&& ef) {
                        return cb(bf, ef);
                    });
                });
            }

            template <template <class...> class Vec>
            auto eframes(FrameType* errtype, auto&& cb) {
                return [=](auto&& bf, bool err) {
                    if (err) {
                        if (errtype) {
                            *errtype = f.type.type_detail();
                        }
                    }
                    bool called = false;
                    auto res = bframe_to_eframe(f, [&](auto&& ef) {
                        called = true;
                        return cb(bf, ef);
                    });
                    if (!called) {
                        if (errtype) {
                            *errtype = f.type.type_detail();
                        }
                    }
                    return res;
                };
            }

        }  // namespace quic::frame
    }      // namespace dnet
}  // namespace utils

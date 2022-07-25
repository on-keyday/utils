/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "types.h"
#include "cast.h"
#include "../mem/buf_write.h"

namespace utils {
    namespace quic {
        namespace frame {

#define WRITE_FIXED(data, length, where)                                              \
    {                                                                                 \
        if (auto err = mem::append_buf<Error>(b, data, length); err != Error::none) { \
            next.frame_error(err, where);                                             \
            return err;                                                               \
        }                                                                             \
    }

#define WRITE(data, length, where)                             \
    {                                                          \
        if (data.size() < length) {                            \
            next.frame_error(Error::not_enough_length, where); \
            return Error::not_enough_length;                   \
        }                                                      \
        WRITE_FIXED(data.c_str(), length, where)               \
    }

#define ENCODE(data, where)                                                      \
    {                                                                            \
        varint::Error verr;                                                      \
        auto err = mem::write_varint<Error>(b, static_cast<tsize>(data), &verr); \
        if (err != Error::none) {                                                \
            if (err == Error::varint_error) {                                    \
                next.varint_error(verr, where);                                  \
            }                                                                    \
            else {                                                               \
                next.frame_error(err, where);                                    \
            }                                                                    \
            return err;                                                          \
        }                                                                        \
    }

            template <class... Ty>
            constexpr bool is_types(types t, Ty... ot) {
                auto eq = [](auto a, auto b) {
                    return a == b;
                };
                return (eq(t, ot) || ...);
            }
#define TYPE(tag, where, ...)                                          \
    {                                                                  \
        if (!is_types(tag.type, __VA_ARGS__)) {                        \
            next.frame_error(Error::consistency_error, where "/type"); \
            return Error::consistency_error;                           \
        }                                                              \
        ENCODE(tag.type, where "/type")                                \
    }

#define CONSISTENCY(cond, where)                           \
    if (!(cond)) {                                         \
        next.frame_error(Error::consistency_error, where); \
        return Error::consistency_error;                   \
    }

            template <class Callbacks>
            Error write_padding(Buffer& b, tsize count, Callbacks&& next) {
                while (count) {
                    constexpr byte s[50]{0};
                    tsize len = count < 50 ? count : 50;
                    WRITE_FIXED(s, len, "write_padding/type")
                    if (len < 50) {
                        break;
                    }
                    count -= 50;
                }
                return Error::none;
            }

            template <class Callbacks>
            Error write_ping(Buffer& b, Callbacks&& next) {
                ENCODE(types::PING, "write_ping/type")
                return Error::none;
            }

            template <class Callbacks>
            Error write_ack(Buffer& b, Ack& ack, Callbacks&& next) {
                TYPE(ack, "write_ack", types::ACK, types::ACK_ECN)
#define E(param) ENCODE(ack.param, "write_ack/" #param)
                CONSISTENCY(ack.ack_range_count <= ack.ack_ranges.size(), "write_ack/ack_range_count")
                E(largest_ack)
                E(ack_delay)
                E(ack_range_count)
                E(first_ack_range)
                for (tsize i = 0; i < ack.ack_range_count; i++) {
                    ENCODE(ack.ack_ranges[i].gap, "write_ack/ack_ranges/gap")
                    ENCODE(ack.ack_ranges[i].ack_len, "write_ack/ack_ranges/gap")
                }
                if (ack.type == types::ACK_ECN) {
                    ENCODE(ack.ecn_counts.ect0_count, "write_ack/ecn_counts/ect0_count")
                    ENCODE(ack.ecn_counts.ect1_count, "write_ack/ecn_counts/ect1_count")
                    ENCODE(ack.ecn_counts.ecn_ce_count, "write_ack/ecn_counts/ecn_ce_count")
                }
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_reset_stream(Buffer& b, ResetStream& reset, Callbacks&& next) {
                TYPE(reset, "write_reset_stream", types::RESET_STREAM)
#define E(param) ENCODE(reset.param, "write_reset_stream/" #param)
                E(stream_id)
                E(application_error_code)
                E(final_size)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_stop_sending(Buffer& b, StopSending& stop, Callbacks&& next) {
                TYPE(stop, "write_stop_sending", types::STOP_SENDING)
#define E(param) ENCODE(stop.param, "write_stop_sending/" #param)
                E(stream_id)
                E(application_error_code)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_crypto(Buffer& b, Crypto& crypto, Callbacks&& next) {
                TYPE(crypto, "write_crypto", types::CRYPTO)
#define E(param) ENCODE(crypto.param, "write_crypto/" #param)
                CONSISTENCY(crypto.length <= crypto.crypto_data.size(), "write_crypto/length")
                E(offset)
                E(length)
                WRITE(crypto.crypto_data, crypto.length, "write_crypto/crypto_data")
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_new_token(Buffer& b, NewToken& new_token, Callbacks&& next) {
                TYPE(new_token, "write_new_token", types::NEW_TOKEN)
#define E(param) ENCODE(new_token.param, "write_new_token/" #param)
                CONSISTENCY(new_token.token_length <= new_token.token.size(), "write_new_token/token_length")
                E(token_length)
                WRITE(new_token.token, new_token.token_length, "write_new_token/token")
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_stream(Buffer& b, Stream& stream, Callbacks&& next) {
                auto st0 = byte(types::STREAM);
                auto st7 = st0 + 0x7;
                auto ty = byte(stream.type);
                if (ty < st0 || st7 < ty) {
                    next.frame_error(Error::consistency_error, "write_stream/type");
                    return Error::consistency_error;
                }
#define E(param) ENCODE(stream.param, "write_stream/" #param)
                E(stream_id)
                if (ty & OFF) {
                    E(offset)
                }
                if (ty & LEN) {
                    CONSISTENCY(stream.length <= stream.stream_data.size(), "write_stream/length")
                    E(length)
                }
                WRITE(stream.stream_data, stream.length, "write_stream/stream_data")
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_max_data(Buffer& b, MaxData& max_data, Callbacks&& next) {
                TYPE(max_data, "write_max_data", types::MAX_DATA)
#define E(param) ENCODE(max_data.param, "write_max_data/" #param)
                E(max_data)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_max_stream_data(Buffer& b, MaxStreamData& max_stream_data, Callbacks&& next) {
                TYPE(max_stream_data, "write_max_stream_data", types::MAX_STREAM_DATA)
#define E(param) ENCODE(max_stream_data.param, "write_max_stream_data/" #param)
                E(stream_id)
                E(max_stream_data)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_max_streams(Buffer& b, MaxStreams& max_streams, Callbacks&& next) {
                TYPE(max_streams, "write_max_streams", types::MAX_STREAMS_BIDI, types::MAX_STREAMS_UNI)
#define E(param) ENCODE(max_streams.param, "write_max_streams/" #param)
                E(max_streams)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_data_blocked(Buffer& b, DataBlocked& data_blocked, Callbacks&& next) {
                TYPE(data_blocked, "write_data_blocked", types::DATA_BLOCKED)
#define E(param) ENCODE(data_blocked.param, "write_data_blocked/" #param)
                E(max_data)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_stream_data_blocked(Buffer& b, StreamDataBlocked& stream_data_blocked, Callbacks&& next) {
                TYPE(stream_data_blocked, "write_stream_data_blocked", types::STREAM_DATA_BLOCKED)
#define E(param) ENCODE(stream_data_blocked.param, "write_stream_data_blocked/" #param)
                E(stream_id)
                E(max_stream_data)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_streams_blocked(Buffer& b, StreamsBlocked& streams_blocked, Callbacks&& next) {
                TYPE(streams_blocked, "write_streams_blocked", types::STREAMS_BLOCKED_BIDI, types::STREAMS_BLOCKED_UNI)
#define E(param) ENCODE(streams_blocked.param, "write_streams_blocked/" #param)
                E(max_streams)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_new_connection_id(Buffer& b, NewConnectionID& new_connection_id, Callbacks&& next) {
                TYPE(new_connection_id, "write_new_connection_id", types::NEW_CONNECTION_ID)
#define E(param) ENCODE(new_connection_id.param, "write_new_connection_id/" #param)
                CONSISTENCY(new_connection_id.length <= new_connection_id.connection_id.size(), "write_new_connection_id/length")
                E(seq_number)
                E(retire_proior_to)
                WRITE_FIXED(&new_connection_id.length, 1, "write_new_connection_id/length")
                WRITE(new_connection_id.connection_id, new_connection_id.length, "write_new_connection_id/connection_id")
                WRITE_FIXED(new_connection_id.stateless_reset_token, 16, "write_new_connection_id/stateless_reset_token")
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_retire_connection_id(Buffer& b, RetireConnectionID& retire, Callbacks&& next) {
                TYPE(retire, "write_retire_connection_id", types::RETIRE_CONNECTION_ID)
#define E(param) ENCODE(retire.param, "write_retire_connection_id/" #param)
                E(seq_number)
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_path_challange(Buffer& b, PathChallange& challange, Callbacks&& next) {
                TYPE(challange, "write_path_challange", types::PATH_CHALLAGE)
                WRITE_FIXED(data, 8, "write_path_challange/data")
                return Error::none;
            }

            template <class Callbacks>
            Error write_path_response(Buffer& b, PathResponse& response, Callbacks&& next) {
                TYPE(response, "write_path_response", types::PATH_RESPONSE)
                WRITE_FIXED(data, 8, "write_path_response/data")
                return Error::none;
            }

            template <class Callbacks>
            Error write_connection_close(Buffer& b, ConnectionClose& close, Callbacks&& next) {
                TYPE(close, "write_connection_close", types::CONNECTION_CLOSE, types::CONNECTION_CLOSE_APP)
#define E(param) ENCODE(close.param, "write_connection_close/" #param)
                CONSISTENCY(close.reason_phrase_length <= close.reason_phrase.size(), "write_connection_close/reason_phrase_length")
                E(error_code)
                if (close.type != types::CONNECTION_CLOSE_APP) {
                    E(frame_type)
                }
                E(reason_phrase_length)
                WRITE(close.reason_phrase, close.reason_phrase_length, "write_connection_close/reason_phrase")
                return Error::none;
#undef E
            }

            template <class Callbacks>
            Error write_handshake_done(Buffer& b, Callbacks&& next) {
                ENCODE(types::HANDSHAKE_DONE, "write_handshake_done/type")
                return Error::none;
            }

            template <class Callbacks>
            Error write_datagram(Buffer& b, Datagram& dgram, Callbacks&& next) {
                TYPE(dgram, "write_datagram", types::DATAGRAM, types::DATAGRAM_LEN)
#define E(param) ENCODE(dgram.param, "write_connection_close/" #param)
                if (dgram.type == types::DATAGRAM_LEN) {
                    CONSISTENCY(dgram.length <= dgram.data.size(), "write_datagram/length")
                    E(length)
                }
                WRITE(dgram.data, dgram.length, "write_datagram/data")
                return Error::none;
#undef E
            }

            template <WriteCallback Callbacks>
            Error write_frame(Buffer& b, Frame* frame, Callbacks&& next) {
                if (!frame) {
                    return Error::invalid_arg;
                }
                auto t = [&](auto... arg) {
                    return is_types(frame->type, arg...);
                };
                auto c = [&]<class T>() {
                    return static_cast<T&>(*frame);
                };
#define CASE(TYPE, FUNC)                                               \
    if (t(TYPE)) {                                                     \
        using TYPE_t = std::remove_pointer_t<type_select<TYPE>::type>; \
        return write_##FUNC(b, c<TYPE_t>(), next)                      \
    }
                constexpr auto s0 = byte(types::STREAM);
                constexpr auto s7 = s0 + 0x7;
                auto ty = byte(f->type);
                if (s0 <= ty && ty <= s7) {
                    return write_stream(b, c<Stream>(), next);
                }
                if (t(types::PADDING)) {
                    return write_padding(b, 1, next);
                }
                if (t(types::PING)) {
                    return write_ping(b, next);
                }
                if (t(types::HANDSHAKE_DONE)) {
                    return write_handshake_done(b, next);
                }
                CASE(types::ACK, ack)
                CASE(types::ACK_ECN, ack)
                CASE(types::RESET_STREAM, reset_stream)
                CASE(types::STOP_SENDING, stop_sending)
                CASE(types::CRYPTO, crypto)
                CASE(types::NEW_TOKEN, new_token)
                CASE(types::MAX_DATA, max_data)
                CASE(types::MAX_STREAM_DATA, max_stream_data)
                CASE(types::MAX_STREAMS_BIDI, max_streams)
                CASE(types::MAX_STREAMS_UNI, max_streams)
                CASE(types::DATA_BLOCKED, data_blocked)
                CASE(types::STREAM_DATA_BLOCKED, stream_data_blocked)
                CASE(types::STREAMS_BLOCKED_BIDI, streams_blocked)
                CASE(types::STREAMS_BLOCKED_UNI, streams_blocked)
                CASE(types::NEW_CONNECTION_ID, new_connection_id)
                CASE(types::RETIRE_CONNECTION_ID, retire_connection_id)
                CASE(types::PATH_CHALLAGE, path_challange)
                CASE(types::PATH_RESPONSE, path_response)
                CASE(types::CONNECTION_CLOSE, connection_close)
                CASE(types::CONNECTION_CLOSE_APP, connection_close)
                CASE(types::DATAGRAM, datagram)
                CASE(types::DATAGRAM_LEN, datagram)
                next.frame_error(Error::unsupported, "write_frame/type");
                return Error::unsupported;
            }
        }  // namespace frame
    }      // namespace quic
}  // namespace utils

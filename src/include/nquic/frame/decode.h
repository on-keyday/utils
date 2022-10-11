/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame - quic frame
#pragma once
#include "../doc.h"
#include "../common/variable_int.h"
#include "types.h"

namespace utils {
    namespace quic {
        namespace frame {

#define DECODE_BASE(NUM, WHERE, BASEWHERE, FRAME)        \
    err = varint::decode(b, &NUM, &red, size, *offset);  \
    if (!ok(err)) {                                      \
        next.varint_error(err, BASEWHERE WHERE, &FRAME); \
        return Error::varint_error;                      \
    }                                                    \
    *offset += red

#define DECODE_INIT() \
    tsize red;        \
    varint::Error err

#define READ_SEQUENCE(TO, LENGTH, WHERE, FRAME)                        \
    {                                                                  \
        TO = next.get_bytes(LENGTH);                                   \
        if (!TO.own() || TO.size() < LENGTH) {                         \
            next.frame_error(Error::memory_exhausted, WHERE, &FRAME);  \
            return Error::memory_exhausted;                            \
        }                                                              \
        if (size < *offset + LENGTH) {                                 \
            next.frame_error(Error::not_enough_length, WHERE, &FRAME); \
            return Error::not_enough_length;                           \
        }                                                              \
        auto target = TO.own();                                        \
        bytes::copy(target, b, *offset + LENGTH, *offset);             \
        *offset += LENGTH;                                             \
    }
#define READ_FIXED(TO, LENGTH, WHERE, FRAME)                           \
    {                                                                  \
        if (size < *offset + LENGTH) {                                 \
            next.frame_error(Error::not_enough_length, WHERE, &FRAME); \
            return Error::not_enough_length;                           \
        }                                                              \
        bytes::copy(TO, b, *offset + LENGTH, *offset);                 \
        *offset += LENGTH;                                             \
    }

            template <class Bytes, class Callbacks>
            Error read_ack_frame(types type, Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
                DECODE_INIT();
                Ack ack{type};
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_ack_frame/", ack)
                DECODE(ack.largest_ack, "largest_ack");
                DECODE(ack.ack_delay, "ack_delay");
                DECODE(ack.ack_range_count, "ack_range_count");
                DECODE(ack.first_ack_range, "first_ack_range");
                allocate::Alloc* a = next.get_alloc();
                ack.ack_ranges = {a};
                if (!ack.ack_ranges.will_append(ack.ack_range_count)) {
                    next.frame_error(Error::memory_exhausted, "read_ack_frame/ack_ranges", &ack);
                    return Error::memory_exhausted;
                }
                for (tsize i = 0; i < ack.ack_range_count; i++) {
                    AckRange range;
                    DECODE(range.gap, "ack_range/gap");
                    DECODE(range.ack_len, "ack_range/ack_len");
                    auto err = ack.ack_ranges.append(std::move(range));
                    assert(err);
                }
                if (type == types::ACK_ECN) {
                    DECODE(ack.ecn_counts.ect0_count, "ecn_counts/ect0_count");
                    DECODE(ack.ecn_counts.ect1_count, "ecn_counts/ect1_count");
                    DECODE(ack.ecn_counts.ecn_ce_count, "ecn_counts/ecn_ce_count");
                }
                return next.frame(&ack);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_reset_stream(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_reset_stream/", reset)
                DECODE_INIT();
                ResetStream reset{types::RESET_STREAM};
                DECODE(reset.stream_id, "stream_id");
                DECODE(reset.application_error_code, "application_error_code");
                DECODE(reset.final_size, "final_size");
                return next.frame(&reset);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_stop_sending(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_stop_sending/", stop)
                DECODE_INIT();
                StopSending stop{types::STOP_SENDING};
                DECODE(stop.stream_id, "stream_id");
                DECODE(stop.application_error_code, "application_error_code");
                return next.frame(&stop);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_crypto(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_crypto/", crypto)
                DECODE_INIT();
                Crypto crypto{types::CRYPTO};
                DECODE(crypto.offset, "offset");
                DECODE(crypto.length, "length");
                READ_SEQUENCE(crypto.crypto_data, crypto.length, "read_crypto/crypto_data", crypto)
                auto res = next.frame(&crypto);
                next.put_bytes(crypto.crypto_data);
                return res;
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_new_token(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_new_token/", new_token)
                DECODE_INIT();
                NewToken new_token{types::NEW_TOKEN};
                DECODE(new_token.token_length, "token_length");
                READ_SEQUENCE(new_token.token, new_token.token_length, "read_new_token/token", new_token)
                auto res = next.frame(&new_token);
                next.put_bytes(new_token.token);
                return res;
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_stream(types types, Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_stream/", stream)
                DECODE_INIT();
                Stream stream{types};
                DECODE(stream.stream_id, "stream_id");
                auto flag = byte(types);
                if (flag & OFF) {
                    DECODE(stream.offset, "offset");
                }
                else {
                    stream.offset = 0;
                }
                if (flag & LEN) {
                    DECODE(stream.length, "length");
                }
                else {
                    stream.length = size - *offset;
                }
                READ_SEQUENCE(stream.stream_data, stream.length, "read_stream/stream_data", stream)
                auto res = next.frame(&stream);
                next.put_bytes(stream.stream_data);
                return res;
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_max_data(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_max_data/", max_data)
                DECODE_INIT();
                MaxData max_data{types::MAX_DATA};
                DECODE(max_data.max_data, "max_data");
                return next.frame(&max_data);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_max_stream_data(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_max_stream_data/", max_stream_data)
                DECODE_INIT();
                MaxStreamData max_stream_data{types::MAX_STREAM_DATA};
                DECODE(max_stream_data.stream_id, "stream_id");
                DECODE(max_stream_data.max_stream_data, "max_stream_data");
                return next.frame(&max_stream_data);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_max_streams(types type, Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_max_streams/", max_streams)
                DECODE_INIT();
                MaxStreams max_streams{type};
                DECODE(max_streams.max_streams, "max_streams");
                return next.frame(&max_streams);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_data_blocked(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_data_blocked/", data_blocked)
                DECODE_INIT();
                DataBlocked data_blocked{types::DATA_BLOCKED};
                DECODE(data_blocked.max_data, "max_data");
                return next.frame(&data_blocked);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_stream_data_blocked(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_stream_data_blocked/", stream_data_blocked)
                DECODE_INIT();
                StreamDataBlocked stream_data_blocked{types::STREAM_DATA_BLOCKED};
                DECODE(stream_data_blocked.stream_id, "stream_id");
                DECODE(stream_data_blocked.max_stream_data, "max_stream_data");
                return next.frame(&stream_data_blocked);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_streams_blocked(types type, Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_streams_blocked/", streams_blocked)
                DECODE_INIT();
                StreamsBlocked streams_blocked{type};
                DECODE(streams_blocked.max_streams, "max_streams");
                return next.frame(&streams_blocked);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_new_connection_id(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_new_connection_id/", nconnid)
                DECODE_INIT();
                NewConnectionID nconnid{types::NEW_CONNECTION_ID};
                DECODE(nconnid.seq_number, "seq_number");
                DECODE(nconnid.retire_proior_to, "retire_proior_to");
                if (*offset >= size) {
                    next.frame_error(Error::not_enough_length, "read_new_connection_id/length", &nconnid);
                    return Error::not_enough_length;
                }
                nconnid.length = b[*offset];
                ++*offset;
                READ_SEQUENCE(nconnid.connection_id, nconnid.length, "read_new_conneciton_id/connection_id", nconnid)
                READ_FIXED(nconnid.stateless_reset_token, 16, "read_new_connection_id/stateless_reset_token", nconnid)
                auto res = next.frame(&nconnid);
                next.put_bytes(nconnid.connection_id);
                return res;
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_retire_connection_id(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_retire_connection_id/", retire)
                DECODE_INIT();
                RetireConnectionID retire{types::RETIRE_CONNECTION_ID};
                DECODE(retire.seq_number, "seq_number");
                return next.frame(&retire);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_path_challange(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_path_challange/", challange)
                DECODE_INIT();
                PathChallange challange{types::PATH_CHALLAGE};
                READ_FIXED(challange.data, 8, "read_path_challange/data", challange);
                return next.frame(&challange);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_path_response(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_path_response/", response)
                DECODE_INIT();
                PathResponse response{types::PATH_RESPONSE};
                READ_FIXED(response.data, 8, "read_path_response/data", response);
                return next.frame(&response);
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_connection_close(types type, Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_connection_close/", close)
                DECODE_INIT();
                ConnectionClose close{type};
                DECODE(close.error_code, "error_code");
                if (type != types::CONNECTION_CLOSE_APP) {
                    tsize ty;
                    DECODE(ty, "frame_type");
                    close.frame_type = types(ty);
                }
                DECODE(close.reason_phrase_length, "reason_phrase_length");
                READ_SEQUENCE(close.reason_phrase, close.reason_phrase_length, "read_connection_close/reason_phrase", close);
                auto res = next.frame(&close);
                next.put_bytes(close.reason_phrase);
                return res;
#undef DECODE
            }

            template <class Bytes, class Callbacks>
            Error read_datagram(types type, Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
#define DECODE(NUM, WHERE) DECODE_BASE(NUM, WHERE, "read_datagram/", dgram)
                DECODE_INIT();
                Datagram dgram{type};
                auto flag = byte(type);
                constexpr auto datagram_LEN = 0x1;
                if (flag & datagram_LEN) {
                    DECODE(dgram.length, "length");
                }
                else {
                    dgram.length = size - *offset;
                }
                READ_SEQUENCE(dgram.data, dgram.length, "read_datagram/data", dgram);
                return next.frame(&dgram);
#undef DECODE
            }

            template <class Bytes>
            tsize read_paddings(Bytes&& b, tsize size, tsize* offset) {
                tsize begin = *offset;
                while (*offset < size && b[*offset] == 0) {
                    ++*offset;
                }
                return *offset - begin;
            }

            template <class Bytes, ReadCallback Callbacks>
            Error read_frame(Bytes&& b, tsize size, tsize* offset, Callbacks&& next) {
                if (!offset || *offset >= size) {
                    return Error::invalid_argument;
                }
                // rfc 9000 says:
                // To ensure simple and efficient implementations of frame parsing,
                // a frame type MUST use the shortest possible encoding.
                // An endpoint MAY treat the receipt of a frame type that uses
                // a longer encoding than necessary as
                // a connection error of type PROTOCOL_VIOLATION.
                types type = types(b[*offset]);
                ++*offset;
                if (type == types::PADDING) {
                    Padding pad{type};
                    return next.frame(&pad);
                }
                if (type == types::PING) {
                    Ping ping{type};
                    return next.frame(&ping);
                }
                if (type == types::HANDSHAKE_DONE) {
                    HandshakeDone done{type};
                    return next.frame(&done);
                }
                if (type == types::ACK ||
                    type == types::ACK_ECN) {
                    return read_ack_frame(type, b, size, offset, next);
                }
                if (type == types::RESET_STREAM) {
                    return read_reset_stream(b, size, offset, next);
                }
                if (type == types::STOP_SENDING) {
                    return read_stop_sending(b, size, offset, next);
                }
                if (type == types::CRYPTO) {
                    return read_crypto(b, size, offset, next);
                }
                if (type == types::NEW_TOKEN) {
                    return read_new_token(b, size, offset, next);
                }
                if (byte(type) >= byte(types::STREAM) &&
                    byte(type) <= byte(types::STREAM) + 0x7) {
                    return read_stream(type, b, size, offset, next);
                }
                if (type == types::MAX_DATA) {
                    return read_max_data(b, size, offset, next);
                }
                if (type == types::MAX_STREAM_DATA) {
                    return read_max_stream_data(b, size, offset, next);
                }
                if (type == types::MAX_STREAMS_BIDI ||
                    type == types::MAX_STREAMS_UNI) {
                    return read_max_streams(type, b, size, offset, next);
                }
                if (type == types::DATA_BLOCKED) {
                    return read_data_blocked(b, size, offset, next);
                }
                if (type == types::STREAM_DATA_BLOCKED) {
                    return read_stream_data_blocked(b, size, offset, next);
                }
                if (type == types::STREAMS_BLOCKED_BIDI ||
                    type == types::STREAMS_BLOCKED_UNI) {
                    return read_streams_blocked(type, b, size, offset, next);
                }
                if (type == types::NEW_CONNECTION_ID) {
                    return read_new_connection_id(b, size, offset, next);
                }
                if (type == types::RETIRE_CONNECTION_ID) {
                    return read_retire_connection_id(b, size, offset, next);
                }
                if (type == types::PATH_CHALLAGE) {
                    return read_path_challange(b, size, offset, next);
                }
                if (type == types::PATH_RESPONSE) {
                    return read_path_response(b, size, offset, next);
                }
                if (type == types::CONNECTION_CLOSE ||
                    type == types::CONNECTION_CLOSE_APP) {
                    return read_connection_close(type, b, size, offset, next);
                }
                if (type == types::DATAGRAM ||
                    type == types::DATAGRAM_LEN) {
                    return read_datagram(type, b, size, offset, next);
                }
                --*offset;
                Frame dummy{type};
                next.frame_error(Error::unsupported, "read_frame/type", &dummy);
                return Error::unsupported;
            }
        }  // namespace frame
    }      // namespace quic
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// types - frame format instance
#pragma once
#include "../mem/vec.h"
#include "../mem/bytes.h"
#include "../common/variable_int.h"
#include <concepts>

namespace utils {
    namespace quic {
        namespace frame {
            enum class Error {
                none,
                invalid_argument,
                varint_error,
                memory_exhausted,
                not_enough_length,
                unsupported,
                consistency_error,
            };

            enum class types {
                PADDING,               // section 19.1
                PING,                  // section 19.2
                ACK,                   // section 19.3
                ACK_ECN,               // with ECN count
                RESET_STREAM,          // section 19.4
                STOP_SENDING,          // section 19.5
                CRYPTO,                // section 19.6
                NEW_TOKEN,             // section 19.7
                STREAM,                // section 19.8
                MAX_DATA = 0x10,       // section 19.9
                MAX_STREAM_DATA,       // section 19.10
                MAX_STREAMS_BIDI,      // section 19.11
                MAX_STREAMS_UNI,       // section 19.11
                DATA_BLOCKED,          // section 19.12
                STREAM_DATA_BLOCKED,   // section 19.13
                STREAMS_BLOCKED_BIDI,  // section 19.14
                STREAMS_BLOCKED_UNI,   // section 19.14
                NEW_CONNECTION_ID,     // section 19.15
                RETIRE_CONNECTION_ID,  // seciton 19.16
                PATH_CHALLAGE,         // seciton 19.17
                PATH_RESPONSE,         // seciton 19.18
                CONNECTION_CLOSE,      // seciton 19.19
                CONNECTION_CLOSE_APP,  // application reason section 19.19
                HANDSHAKE_DONE,        // seciton 19.20
                DATAGRAM = 0x30,       // rfc 9221 datagram
                DATAGRAM_LEN,
            };

            constexpr bool is_STREAM(types typ) {
                auto t = int(typ);
                constexpr auto v = int(types::STREAM);
                return v <= t && t <= v + 0xf;
            }

            struct Frame {
                types type;
            };

            struct Padding : Frame {
            };

            struct Ping : Frame {
            };

            struct AckRange {
                tsize gap;
                tsize ack_len;
            };

            struct ECNCounts {
                tsize ect0_count;
                tsize ect1_count;
                tsize ecn_ce_count;
            };

            struct Ack : Frame {
                tsize largest_ack;
                tsize ack_delay;
                tsize ack_range_count;
                tsize first_ack_range;
                mem::Vec<AckRange> ack_ranges;
                ECNCounts ecn_counts;
            };

            struct ResetStream : Frame {
                tsize stream_id;
                tsize application_error_code;
                tsize final_size;
            };

            struct StopSending : Frame {
                tsize stream_id;
                tsize application_error_code;
            };

            struct Crypto : Frame {
                tsize offset;
                tsize length;
                bytes::Bytes crypto_data;
            };

            struct NewToken : Frame {
                tsize token_length;
                bytes::Bytes token;
            };

            struct Stream : Frame {
                tsize stream_id;
                tsize offset;
                tsize length;
                bytes::Bytes stream_data;
            };

            enum StreamFlag {
                FIN = 0x1,
                LEN = 0x2,
                OFF = 0x4,
            };

            struct MaxData : Frame {
                tsize max_data;
            };

            struct MaxStreamData : Frame {
                tsize stream_id;
                tsize max_stream_data;
            };

            struct MaxStreams : Frame {
                tsize max_streams;
            };

            struct DataBlocked : Frame {
                tsize max_data;
            };

            struct StreamDataBlocked : Frame {
                tsize stream_id;
                tsize max_stream_data;
            };

            struct StreamsBlocked : Frame {
                tsize max_streams;
            };

            struct NewConnectionID : Frame {
                tsize seq_number;
                tsize retire_proior_to;
                byte length;
                bytes::Bytes connection_id;
                byte stateless_reset_token[16];
            };

            struct RetireConnectionID : Frame {
                tsize seq_number;
            };

            struct PathChallange : Frame {
                byte data[8];
            };

            struct PathResponse : Frame {
                byte data[8];
            };

            struct ConnectionClose : Frame {
                tsize error_code;
                types frame_type;
                tsize reason_phrase_length;
                bytes::Bytes reason_phrase;
            };

            struct HandshakeDone : Frame {
            };

            struct Datagram : Frame {
                tsize length;
                bytes::Bytes data;
            };

            template <class T>
            concept ReadCallback = requires(T t) {
                { t.frame(std::declval<Frame*>()) } -> std::same_as<Error>;
                {t.frame_error(Error{}, std::declval<const char*>(), std::declval<Frame*>())};
                {t.varint_error(varint::Error{}, std::declval<const char*>(), std::declval<Frame*>())};
                { t.get_alloc() } -> std::same_as<allocate::Alloc*>;
                { t.get_bytes(tsize{}) } -> std::same_as<bytes::Bytes>;
                {t.put_bytes(std::declval<bytes::Bytes&>())};
            };
            template <class T>
            concept WriteCallback = requires(T t) {
                {t.frame_error(Error{}, std::declval<const char*>())};
                {t.varint_error(varint::Error{}, std::declval<const char*>())};
            };

            using Buffer = bytes::Buffer;

        }  // namespace frame
    }      // namespace quic
}  // namespace utils

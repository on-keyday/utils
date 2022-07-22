/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// types - frame format instance
#pragma once
#include "../doc.h"
#include "../mem/vec.h"
#include "../mem/bytes.h"
#include "../common/variable_int.h"

namespace utils {
    namespace quic {
        namespace frame {
            enum class Error {
                none,
                invalid_arg,
                varint_error,
                memory_exhausted,
                not_enough_length,
                unsupported,
                consistency_error,
            };

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

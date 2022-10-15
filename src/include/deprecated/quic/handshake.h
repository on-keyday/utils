/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic_handshake - quic handshake
#pragma once

#include "doc.h"

namespace utils {
    namespace quic {
        namespace handshake {
            enum class Error {
                none,
            };

            struct Flow {
                Direction direction;
                packet::number packet_number;
                packet::types packet;
                frame::types frames[3];
            };

            // Figure 5: Example 1-RTT Handshake
            constexpr Flow one_rtt_flow[]{
                {client_to_server, 0, packet::types::Initial, {frame::types::CRYPTO}},
                {server_to_client, 0, packet::types::Initial, {frame::types::CRYPTO, frame::types::ACK}},
                {server_to_client, 0, packet::types::HandShake, {frame::types::CRYPTO}},
                {server_to_client, 0, packet::types::OneRTT, {frame::types::STREAM, frame::types::ACK}},
                {client_to_server, 1, packet::types::Initial, {frame::types::ACK}},
                {client_to_server, 0, packet::types::HandShake, {frame::types::CRYPTO, frame::types::ACK}},
                {client_to_server, 0, packet::types::OneRTT, {frame::types::STREAM, frame::types::ACK}},
                {server_to_client, 1, packet::types::HandShake, {frame::types::ACK}},
                {server_to_client, 1, packet::types::OneRTT, {frame::types::HANDSHAKE_DONE, frame::types::STREAM, frame::types::ACK}},
            };

            // Figure 6: Example 0-RTT Handshake
            constexpr Flow zero_rtt_flow[]{
                {client_to_server, 0, packet::types::Initial, {frame::types::CRYPTO}},
                {client_to_server, 0, packet::types::ZeroRTT, {frame::types::STREAM}},
                {server_to_client, 0, packet::types::Initial, {frame::types::CRYPTO, frame::types::ACK}},
                {server_to_client, 0, packet::types::HandShake, {frame::types::CRYPTO}},
                {server_to_client, 0, packet::types::OneRTT, {frame::types::STREAM, frame::types::ACK}},
                {client_to_server, 1, packet::types::Initial, {frame::types::ACK}},
                {client_to_server, 0, packet::types::HandShake, {frame::types::CRYPTO, frame::types::ACK}},
                {client_to_server, 1, packet::types::OneRTT, {frame::types::STREAM, frame::types::ACK}},
            };

            Error one_rtt_handshake() {
                return Error::none;
            }

            // Until a packet is received from the server, the client MUST use the same Destination Connection ID value on all packets in this connection.
            // The Destination Connection ID field from the first Initial packet sent by a client
            // is used to determine packet protection keys for Initial packets.
            // These keys change after receiving a Retry packet; see Section 5.2 of [QUIC-TLS].
            constexpr auto UnspecDstFieldLeastLen = 8;

            struct SrcDst {
                Direction direction;
                packet::types packet;
                const byte* dst;
                const byte* src;
            };
            // Figure 7 : Use of Connection IDs in a Handshake
            constexpr SrcDst connid_flow_1[]{
                // client sets:
                // initial_source_connection_id = C1
                {client_to_server, packet::types::Initial, "S1", "C1"},
                // server validates whether initial_source_connection_id exists
                // and if not, it's TRANSPORT_PARAMETER_ERROR connection error
                // server sets:
                // original_destination_connection_id = S1
                // initial_source_connection_id = S3
                {server_to_client, packet::types::Initial, "C1", "S3"},
                // client validates whether
                // initial_source_connection_id exists &&
                // DstConnectionID==C1 &&
                // original_destination_connection_id == S1
                // ... and other negotiations
                {client_to_server, packet::types::OneRTT, "S3"},
                {server_to_client, packet::types::OneRTT, "C1"},
            };

            // Figure 8: Use of Connection IDs in a Handshake with Retry
            constexpr SrcDst connid_flow_2[]{
                // client sets:
                // initial_source_connection_id = C1
                {client_to_server, packet::types::Initial, "S1", "C1"},
                // because of some reason,server sends Retry packet
                // server saves C1 and S1
                {server_to_client, packet::types::Retry, "C1", "S2"},
                // client retry to send Initial packet
                // client sets:
                // initial_source_connection_id = C1
                {client_to_server, packet::types::Initial, "S2", "C1"},
                // server accepts C1 and restore saved data
                // server sets:
                // original_destination_connection_id = S1
                // retry_source_connection_id = S2
                // initial_source_connection_id = S3
                {server_to_client, packet::types::Initial, "C1", "S2"},
                // ... and other negotiations
                {client_to_server, packet::types::OneRTT, "S3"},
                {server_to_client, packet::types::OneRTT, "C1"},
            };

        }  // namespace handshake
    }      // namespace quic
}  // namespace utils

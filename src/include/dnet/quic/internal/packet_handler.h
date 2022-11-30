/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_handler - packet handler
#pragma once
#include "quic_contexts.h"
#include "../packet/packet.h"

namespace utils {
    namespace dnet {
        namespace quic::handler {
            struct PacketState {
                PacketType type;
                bool (*is_ok)(FrameType);
                ack::PacketNumberSpace space;
                crypto::EncryptionLevel enc_level;
            };

            error::Error recv_QUIC_packets(QUICContexts* q, ByteLen src);
            error::Error on_initial_packet(QUICContexts* q, ByteLen src, packet::InitialPacketCipher& packet);
            error::Error on_handshake_packet(QUICContexts* q, ByteLen src, packet::HandshakePacketCipher& packet);
            error::Error on_retry_packet(QUICContexts* q, ByteLen src, packet::RetryPacket&);
            error::Error on_0rtt_packet(QUICContexts* q, ByteLen src, packet::ZeroRTTPacketCipher&);
            error::Error on_1rtt_packet(QUICContexts* q, ByteLen src, packet::OneRTTPacketCipher&);
            error::Error on_stateless_reset(QUICContexts* q, ByteLen src, packet::StatelessReset&);
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_handler - packet handler
#pragma once
#include "../../dll/dllh.h"
#include "quic_contexts.h"
#include "../packet/wire.h"

namespace utils {
    namespace dnet {
        namespace quic::handler {
            struct PacketState {
                PacketType type;
                bool (*is_ok)(FrameType);
                ack::PacketNumberSpace space;
                crypto::EncryptionLevel enc_level;
            };

            dnet_dll_export(error::Error) recv_QUIC_packets(QUICContexts* q, ByteLen src);
            dnet_dll_export(error::Error) on_initial_packet(QUICContexts* q, ByteLen src, packet::InitialPacketCipher& packet);
            dnet_dll_export(error::Error) on_handshake_packet(QUICContexts* q, ByteLen src, packet::HandshakePacketCipher& packet);
            dnet_dll_export(error::Error) on_retry_packet(QUICContexts* q, ByteLen src, packet::RetryPacket&);
            dnet_dll_export(error::Error) on_0rtt_packet(QUICContexts* q, ByteLen src, packet::ZeroRTTPacketCipher&);
            dnet_dll_export(error::Error) on_1rtt_packet(QUICContexts* q, ByteLen src, packet::OneRTTPacketCipher&);
            dnet_dll_export(error::Error) on_stateless_reset(QUICContexts* q, ByteLen src, packet::StatelessReset&);
            dnet_dll_export(error::Error) handle_frames(QUICContexts* q, ByteLen raw_frames, QPacketNumber packet_number, PacketState state);
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

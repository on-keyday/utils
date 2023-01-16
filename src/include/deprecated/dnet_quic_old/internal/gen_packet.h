/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../bytelen.h"
#include "../../closure.h"
#include "quic_contexts.h"

namespace utils {
    namespace dnet {
        namespace quic::handler {
            struct PacketArg {
                ByteLen srcID;
                ByteLen dstID;
                ByteLen token;
                ByteLen origDstID;
                size_t payload_len = 0;
                size_t reqlen = 0;
                std::uint32_t version = 0;
                bool spin = false;
                bool key_phase = false;

                bool ack_eliciting = false;
                bool in_flight = false;
            };
            using GenPayloadCB = closure::Callback<error::Error, PacketArg&, WPacket&, size_t /*space*/, size_t /*packet number*/>;
            dnet_dll_export(error::Error) generate_initial_packet(QUICContexts* p, PacketArg arg, GenPayloadCB cb);
            dnet_dll_export(error::Error) generate_handshake_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload);
            dnet_dll_export(error::Error) generate_onertt_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload);

            dnet_dll_export(error::Error) generate_packets(QUICContexts* q);

        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

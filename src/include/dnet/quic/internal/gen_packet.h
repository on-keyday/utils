/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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
            };
            using GenPayloadCB = closure::Callback<void, WPacket&, size_t>;
            error::Error generate_initial_packet(QUICContexts* p, PacketArg arg, GenPayloadCB cb);

        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

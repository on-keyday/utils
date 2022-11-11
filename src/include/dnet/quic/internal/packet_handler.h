/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_handler - packet handler
#pragma once
#include "quic_contexts.h"
#include "../packet.h"

namespace utils {
    namespace dnet {
        namespace quic::handler {
            bool on_initial_packet(QUICContexts* q, packet::InitialPacketCipher& packet);
        }
    }  // namespace dnet
}  // namespace utils

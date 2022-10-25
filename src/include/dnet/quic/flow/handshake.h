/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// handshake - QUIC Handshake Packet
#pragma once
#include "../packet.h"

namespace utils {
    namespace dnet {
        namespace quic::flow {
            constexpr CryptoPacketInfo parseHandshakePartial(ByteLen b, auto&& validate) {
                dnet::quic::HandshakePacketPartial partial;
                auto copy = b;
                if (!partial.parse(copy)) {
                    return {};
                }
                if (!validate(partial.version.as<std::uint32_t>(), std::as_const(partial.srcID), std::as_const(partial.dstID))) {
                    return {};
                }
                auto plen = partial.packet_len();
                auto len = partial.length.qvarint();
                CryptoPacketInfo info;
                info.head = b.resized(plen);
                info.payload = b.forward(plen).resized(len);
                return info;
            }
        }  // namespace quic::flow
    }      // namespace dnet
}  // namespace utils

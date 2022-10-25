/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../bytelen.h"
#include "../packet.h"

namespace utils {
    namespace dnet {
        namespace quic::flow {
            constexpr CryptoPacketInfo parseOneRTTPartial(ByteLen b, auto& dstIDLen) {
                OneRTTPacketPartial partial;
                auto copy = b;
                if (!partial.parse(copy, dstIDLen)) {
                    return {};
                }
                auto plen = partial.packet_len();
                CryptoPacketInfo info;
                info.head = b.resized(plen);
                info.payload = b.forward(plen);
                return info;
            }
        }  // namespace quic::flow
    }      // namespace dnet
}  // namespace utils

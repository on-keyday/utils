/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// wire - packets
#pragma once
#include "versionnego.h"
#include "initial.h"
#include "handshake_0rtt.h"
#include "retry.h"
#include "onertt.h"
#include "stateless_reset.h"
#include "crypto.h"
#include "summary.h"

namespace utils {
    namespace fnet::quic::packet {
        template <class T>
        constexpr bool is_CryptoLongPacketPlain_v =
            std::is_same_v<T, InitialPacketPlain> ||
            std::is_same_v<T, HandshakePacketPlain> ||
            std::is_same_v<T, ZeroRTTPacketPlain>;

        template <class T>
        constexpr bool is_CryptoLongPacketCipher_v =
            std::is_same_v<T, InitialPacketCipher> ||
            std::is_same_v<T, HandshakePacketCipher> ||
            std::is_same_v<T, ZeroRTTPacketCipher>;

        template <class T>
        constexpr bool is_LongPacket_v =
            is_CryptoLongPacketPlain_v<T> || std::is_same_v<T, RetryPacket>;

        template <class T>
        constexpr bool is_CryptoPacketPlain_v =
            is_CryptoLongPacketPlain_v<T> || std::is_same_v<T, OneRTTPacketPlain>;
    }  // namespace fnet::quic::packet
}  // namespace utils

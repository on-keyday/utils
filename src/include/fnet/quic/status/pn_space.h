/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../types.h"

namespace futils {
    namespace fnet::quic::status {
        enum class PacketNumberSpace : byte {
            initial,
            handshake,
            application,
            no_space = 0xff,
        };

        constexpr const char* to_string(PacketNumberSpace space) noexcept {
            switch (space) {
                case PacketNumberSpace::initial:
                    return "initial";
                case PacketNumberSpace::handshake:
                    return "handshake";
                case PacketNumberSpace::application:
                    return "application";
                default:
                    return "no_space";
            }
        }

        constexpr int space_to_index(PacketNumberSpace space) {
            return int(space);
        }

        constexpr PacketNumberSpace from_packet_type(PacketType typ) {
            switch (typ) {
                case PacketType::Initial:
                    return PacketNumberSpace::initial;
                case PacketType::Handshake:
                    return PacketNumberSpace::handshake;
                case PacketType::ZeroRTT:
                case PacketType::OneRTT:
                    return PacketNumberSpace::application;
                default:
                    return PacketNumberSpace::no_space;
            }
        }
    }  // namespace fnet::quic::status
}  // namespace futils
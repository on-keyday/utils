/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// pn_space - packet number space
#pragma once
#include "../types.h"

namespace utils {
    namespace dnet::quic::ack {
        enum class PacketNumberSpace {
            initial,
            handshake,
            application,
            no_space = -1,
        };

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
    }  // namespace dnet::quic::ack
}  // namespace utils

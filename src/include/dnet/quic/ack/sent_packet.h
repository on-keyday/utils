/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// sent_packet - sent packet
#pragma once
#include <cstdint>
#include "../../boxbytelen.h"
#include <vector>
#include "../../dll/allocator.h"
#include <compare>
#include "rtt.h"
#include "../../easy/vector.h"
#include "packet_number.h"

namespace utils {
    namespace dnet {
        namespace quic::ack {

            enum class PacketNumberSpace {
                initial,
                handshake,
                application,
                no_space = -1,
            };

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

            /*
            enum class PacketNumber : std::uint64_t {
                infinite = ~std::uint64_t(0),
            };*/

            struct SentPacket {
                BoxByteLen sent_plain;
                PacketType type = PacketType::Unknown;
                PacketNumber packet_number{0};
                size_t sent_bytes = 0;
                bool ack_eliciting = false;
                bool in_flight = false;
                bool skiped = false;
                time_t time_sent = invalid_time;
            };

            struct RemovedPackets {
                easy::Vec<SentPacket> rem;
                error::Error push_back(SentPacket&& sent) {
                    return rem.push_back(std::move(sent));
                }

                SentPacket* begin() {
                    return rem.data();
                }

                SentPacket* end() {
                    return rem.data() + rem.size();
                }

                SentPacket& operator[](size_t i) {
                    return rem[i];
                }

                bool empty() const {
                    return rem.empty();
                }
            };

        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

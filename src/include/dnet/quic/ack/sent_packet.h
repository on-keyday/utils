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

namespace utils {
    namespace dnet {
        namespace quic::ack {

            enum class PacketNumberSpace {
                initial,
                handshake,
                application,
            };

            enum class PacketNumber : std::uint64_t {
                infinite = ~std::uint64_t(0),
            };

            constexpr auto operator<=>(const PacketNumber& a, const PacketNumber& b) {
                return std::uint64_t(a) <=> std::uint64_t(b);
            }

            constexpr auto operator<=>(const PacketNumber& a, size_t b) {
                return std::uint64_t(a) <=> std::uint64_t(b);
            }

            constexpr auto operator+(const PacketNumber& a, size_t b) {
                return PacketNumber(std::uint64_t(a) + b);
            }

            struct SentPacket {
                BoxByteLen sent_plain;
                PacketType type = PacketType::Unknown;
                PacketNumber packet_number{0};
                size_t sent_bytes = 0;
                bool ack_eliciting = false;
                bool in_flight = false;
                time_t time_sent = -1;
            };

            using RemovedPackets = std::vector<SentPacket, glheap_allocator<SentPacket>>;

        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

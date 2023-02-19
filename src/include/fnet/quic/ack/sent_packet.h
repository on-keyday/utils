/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// sent_packet - sent packet
#pragma once
#include <cstdint>
#include "../../std/vector.h"
#include <compare>
#include "../time.h"
#include "../packet_number.h"
#include "ack_lost_record.h"
#include "../packet/summary.h"
#include "ack_waiters.h"

namespace utils {
    namespace fnet::quic::ack {

        struct SentPacket {
            PacketType type = PacketType::Unknown;
            packetnum::Value packet_number;
            ACKWaiters waiters;
            std::uint64_t sent_bytes = 0;
            packet::PacketStatus status;
            bool is_mtu_probe = false;
            bool skiped = false;
            time::Time time_sent = time::invalid;
        };

        using RemovedPackets = slib::vector<SentPacket>;

    }  // namespace fnet::quic::ack
}  // namespace utils

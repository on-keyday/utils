/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
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

namespace futils {
    namespace fnet::quic::ack {

        struct SentPacket {
            PacketType type = PacketType::Unknown;
            packet::PacketStatus status;
            packetnum::Value packet_number;
            std::weak_ptr<ACKLostRecord> waiter;
            std::uint64_t sent_bytes = 0;
            time::Time time_sent = time::invalid;
            packetnum::Value largest_ack = packetnum::infinity;
        };

        using RemovedPackets = slib::vector<SentPacket>;

    }  // namespace fnet::quic::ack
}  // namespace futils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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
#include "pn_space.h"

namespace utils {
    namespace dnet::quic::ack {

        struct SentPacket {
            PacketType type = PacketType::Unknown;
            packetnum::Value packet_number;
            slib::vector<std::weak_ptr<ACKLostRecord>> waiters;
            std::uint64_t sent_bytes = 0;
            bool ack_eliciting = false;
            bool in_flight = false;
            bool skiped = false;
            time::Time time_sent = time::invalid;
        };

        using RemovedPackets = slib::vector<SentPacket>;

    }  // namespace dnet::quic::ack
}  // namespace utils

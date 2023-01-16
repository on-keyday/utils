/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// summary - packet summary
#pragma once
#include "../types.h"
#include "../../../view/iovec.h"
#include "../packet_number.h"

namespace utils {
    namespace dnet::quic::packet {
        struct PacketSummary {
            PacketType type;
            std::uint32_t version = 0;
            view::rvec dstID;
            view::rvec srcID;
            view::rvec token;
            packetnum::Value packet_number;
            bool key = false;
            bool spin = false;
        };

        struct PacketStatus {
            bool is_ack_eliciting = false;
            bool is_byte_counted = false;

            constexpr void apply(FrameType type) {
                is_ack_eliciting = is_ack_eliciting || is_ACKEliciting(type);
                is_byte_counted = is_byte_counted || is_ByteCounted(type);
            }
        };

    }  // namespace dnet::quic::packet
}  // namespace utils

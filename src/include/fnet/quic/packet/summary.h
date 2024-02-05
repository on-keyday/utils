/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// summary - packet summary
#pragma once
#include "../types.h"
#include "../../../view/iovec.h"
#include "../packet_number.h"

namespace futils {
    namespace fnet::quic::packet {
        struct PacketSummary {
            PacketType type;
            std::uint32_t version = 0;
            view::rvec dstID;
            view::rvec srcID;
            view::rvec token;
            packetnum::Value packet_number;
            // largest ack packet number on ACKFrame
            packetnum::Value largest_ack = packetnum::infinity;
            bool key_bit = false;
            bool spin = false;
        };

        struct PacketStatus {
           private:
            enum : byte {
                flag_is_ack_eliciting = 0x1,
                flag_is_byte_counted = 0x2,
                flag_is_non_path_probe = 0x4,
                flag_is_mtu_probe = 0x8,
                flag_is_skip = 0x10,
            };
            byte flags = 0;

           public:
            constexpr bool is_ack_eliciting() const noexcept {
                return flags & flag_is_ack_eliciting;
            }

            constexpr bool is_byte_counted() const noexcept {
                return flags & flag_is_byte_counted;
            }

            constexpr bool is_non_path_probe() const noexcept {
                return flags & flag_is_non_path_probe;
            }

            constexpr bool is_mtu_probe() const noexcept {
                return flags & flag_is_non_path_probe;
            }

            constexpr bool is_skipped() const noexcept {
                return flags & flag_is_skip;
            }

            constexpr void set_mtu_probe() noexcept {
                flags |= flag_is_mtu_probe;
            }

            constexpr void set_skipped() noexcept {
                flags |= flag_is_skip;
            }

            constexpr void add_frame(FrameType type) noexcept {
                auto add_flag = [&](bool f, byte flag) {
                    flags |= f ? flag : 0;
                };
                add_flag(is_ack_eliciting() || is_ACKEliciting(type), flag_is_ack_eliciting);
                add_flag(is_byte_counted() || is_ByteCounted(type), flag_is_byte_counted);
                add_flag(is_non_path_probe() || !is_PathProbe(type), flag_is_non_path_probe);
            }
        };

    }  // namespace fnet::quic::packet
}  // namespace futils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// pto_loss_param - pto and loss params
#pragma once
#include "constant.h"
#include "../time.h"
#include "pn_space.h"
#include "rtt.h"
#include "handshake.h"

namespace utils {
    namespace fnet::quic::status {

        struct PTOStatus {
           private:
            std::uint64_t pto_count = 0;
            std::uint64_t probe_required = 0;
            PacketNumberSpace pto_space = PacketNumberSpace::no_space;

           public:
            constexpr void reset() {
                pto_count = 0;
                probe_required = 0;
                pto_space = PacketNumberSpace::no_space;
            }

            constexpr std::uint64_t pto_exponent() const {
                return time::utime_t(1) << pto_count;
            }

            constexpr time::time_t probe_timeout_duration(const InternalConfig& config, const RTT& rtt) const {
                return rtt.smoothed_rtt() + max_(
                                                time::time_t(rtt.rttvar()) << 2,  // rtt.rttvar() * 4
                                                config.clock.to_clock_granurarity(1)) *
                                                pto_exponent();
            }

            constexpr time::time_t probe_timeout_duration_with_max_ack_delay(const InternalConfig& config, const RTT& rtt) const {
                return probe_timeout_duration(config, rtt) + rtt.max_ack_delay() * pto_exponent();
            }

            constexpr bool is_probe_required(PacketNumberSpace space) const {
                return pto_space == space && probe_required > 0;
            }

            constexpr void on_packet_sent(bool ack_eliciting) {
                if (ack_eliciting && probe_required > 0) {
                    probe_required--;
                }
            }

            constexpr void on_pto_no_flight(PacketNumberSpace space) {
                probe_required++;
                pto_space = space;
                pto_count++;
            }

            constexpr void on_pto_timeout(PacketNumberSpace space) {
                probe_required += 2;
                pto_space = space;
                pto_count++;
            }

            constexpr void on_ack_received(const HandshakeStatus& status) {
                probe_required = 0;
                if (status.peer_completed_address_validation()) {
                    pto_count = 0;
                }
            }

            constexpr void on_packet_number_space_discard() {
                pto_space = PacketNumberSpace::no_space;
                probe_required = 0;
                pto_count = 0;
            }
        };

    }  // namespace fnet::quic::status
}  // namespace utils
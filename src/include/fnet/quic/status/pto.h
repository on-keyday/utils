/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
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

namespace futils {
    namespace fnet::quic::status {

        inline time::time_t calc_probe_timeout_duration(time::time_t smoothed_rtt, time::time_t rttvar, const time::Clock& clock, std::uint64_t pto_exponent) {
            return smoothed_rtt + max_(rttvar << 2,  // rttvar*4
                                       clock.to_clock_granularity(1) * pto_exponent);
        }

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
                return calc_probe_timeout_duration(rtt.smoothed_rtt(), rtt.rttvar(), config.clock, pto_exponent());
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

            constexpr void on_retry_received(const InternalConfig& config, RTT& rtt, time::Time first_sent_time) {
                if (pto_count == 0) {
                    rtt.sample_rtt(config, first_sent_time, 0);
                }
                pto_count = 0;
            }
        };

    }  // namespace fnet::quic::status
}  // namespace futils

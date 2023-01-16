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

namespace utils {
    namespace dnet::quic::ack {

        struct LossDetectFlags {
            bool has_sent_handshake = false;
            bool handshake_confirmed = false;
            bool is_server = false;
            bool has_recived_handshake = false;
            bool has_recived_handshake_ack = false;
            std::uint64_t sent_bytes = 0;
            std::uint64_t recv_bytes = 0;

            constexpr bool is_at_anti_amplification_limit() const {
                if (peer_address_validated()) {
                    return false;
                }
                return sent_bytes >= amplification_factor * recv_bytes;
            }

            constexpr bool peer_address_validated() const {
                if (!is_server) {
                    return true;  // server address is always validated
                }
                return has_recived_handshake;
            }

            constexpr bool peer_completed_address_validation() const {
                if (is_server) {
                    return true;
                }
                return has_recived_handshake_ack || handshake_confirmed;
            }
        };

        struct PTOParams {
            std::uint64_t pto_count = 0;
            std::uint64_t probe_required = 0;
            PacketNumberSpace pto_space = PacketNumberSpace::no_space;

            constexpr time::time_t probe_timeout_duration(time::Clock& clock, RTT& rtt) const {
                return rtt.smoothed_rtt + max_(time::time_t(rtt.rttvar) << 2, clock.granularity) * (time::utime_t(1) << pto_count);
            }

            bool is_probe_required(PacketNumberSpace space) const {
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

            constexpr void on_ack_recved(const LossDetectFlags& flags) {
                probe_required = 0;
                if (flags.peer_completed_address_validation()) {
                    pto_count = 0;
                }
            }

            constexpr void on_pn_space_discard() {
                pto_space = PacketNumberSpace::no_space;
                probe_required = 0;
                pto_count = 0;
            }
        };

        struct LossDetectParams {
            std::uint64_t packet_order_threshold = default_packet_threshold;
            Ratio time_threshold = default_time_threshold;

            constexpr time::Time loss_delay(time::Clock& clock, RTT& rtt) const {
                if (time_threshold.den == 0) {
                    return time::invalid;
                }
                auto k = (time_threshold.num * min_(rtt.smoothed_rtt, rtt.latest_rtt));
                decltype(k) candidate;
                // HACK(on-keyday): recommended is 9/8 so optimize for den == 8
                if (time_threshold.den == 8) {
                    candidate = k >> 3;
                }
                else {
                    candidate = k / time_threshold.den;
                }
                if (candidate < 0) {
                    return time::invalid;  // BUG overflow
                }
                return max_(candidate, clock.granularity);
            }
        };
    }  // namespace dnet::quic::ack
}  // namespace utils

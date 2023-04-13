/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "config.h"
#include "../packet_number.h"
#include "../time.h"
#include "../packet/summary.h"

namespace utils {
    namespace fnet::quic::status {

        struct PacketNumberIssuer {
           private:
            std::int64_t issue_packet_number = 0;
            std::int64_t highest_sent = -1;
            std::uint64_t largest_acked_packet = -1;
            time::Time last_ack_eliciting_packet_sent_time_ = 0;
            std::uint64_t ack_eliciting_in_flight_sent_packet_count = 0;

            constexpr void decrement_ack_eliciting_in_flight(packet::PacketStatus status) {
                if (status.is_ack_eliciting && status.is_byte_counted) {
                    ack_eliciting_in_flight_sent_packet_count--;
                }
            }

           public:
            constexpr void reset() {
                issue_packet_number = 0;
                highest_sent = -1;
                largest_acked_packet = -1;
                last_ack_eliciting_packet_sent_time_ = 0;
                ack_eliciting_in_flight_sent_packet_count = 0;
            }

            constexpr packetnum::Value next_packet_number() const {
                return issue_packet_number;
            }

            constexpr void consume_packet_number() {
                issue_packet_number++;
            }

            constexpr packetnum::Value largest_acked_packet_number() const {
                return largest_acked_packet;
            }

            constexpr std::pair<std::int64_t, std::int64_t> on_packet_sent(
                const InternalConfig& config,
                packetnum::Value pn, packet::PacketStatus status) {
                const auto cmp = std::int64_t(pn.value);
                if (cmp <= highest_sent || issue_packet_number < cmp) {
                    return {-1, -1};
                }
                const auto range_begin = highest_sent + 1;
                highest_sent = pn.value;
                if (status.is_ack_eliciting) {
                    last_ack_eliciting_packet_sent_time_ = config.clock.now();
                }
                if (status.is_ack_eliciting && status.is_byte_counted) {
                    ack_eliciting_in_flight_sent_packet_count++;
                }
                return {range_begin, pn.value};
            }

            constexpr void on_ack_received(packetnum::Value new_largest_ack_pn) {
                if (largest_acked_packet == -1 ||
                    largest_acked_packet < new_largest_ack_pn) {
                    largest_acked_packet = new_largest_ack_pn;
                }
            }

            constexpr void on_packet_number_space_discard() {
                last_ack_eliciting_packet_sent_time_ = 0;
                ack_eliciting_in_flight_sent_packet_count = 0;
            }

            constexpr void on_packet_ack(packet::PacketStatus status) {
                decrement_ack_eliciting_in_flight(status);
            }

            constexpr void on_packet_lost(packet::PacketStatus status) {
                decrement_ack_eliciting_in_flight(status);
            }

            constexpr bool no_ack_eliciting_in_flight() const {
                return ack_eliciting_in_flight_sent_packet_count == 0;
            }

            constexpr time::Time last_ack_eliciting_packet_sent_time() const {
                return last_ack_eliciting_packet_sent_time_;
            }

            constexpr void on_retry_received() {
                last_ack_eliciting_packet_sent_time_ = 0;
                ack_eliciting_in_flight_sent_packet_count = 0;
                highest_sent = -1;
                largest_acked_packet = -1;
            }
        };

        struct PacketNumberAcceptor {
           private:
            std::uint64_t largest_recv_packet = 0;

           public:
            constexpr void on_packet_processed(packetnum::Value pn) {
                if (largest_recv_packet < pn.value) {
                    largest_recv_packet = pn.value;
                }
            }

            constexpr std::uint64_t largest_received_packet_number() const {
                return largest_recv_packet;
            }
        };

    }  // namespace fnet::quic::status
}  // namespace utils

/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "config.h"
#include "../packet_number.h"
#include "../time.h"
#include "../packet/summary.h"

namespace futils {
    namespace fnet::quic::status {

        struct PacketNumberIssuer {
           private:
            std::int64_t issue_packet_number = 0;
            std::int64_t highest_sent = -1;
            packetnum::Value largest_acked_packet = packetnum::infinity;
            time::Time last_ack_eliciting_packet_sent_time_ = 0;
            std::uint64_t ack_eliciting_in_flight_sent_packet_count = 0;

            constexpr void decrement_ack_eliciting_in_flight(packet::PacketStatus status) {
                if (status.is_ack_eliciting() && status.is_byte_counted()) {
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

            constexpr void on_connection_migration() {
                largest_acked_packet = -1;
                highest_sent = -1;
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

            constexpr expected<std::pair<std::int64_t, std::int64_t>> on_packet_sent(
                const InternalConfig& config,
                packetnum::Value pn, packet::PacketStatus status) {
                const auto cmp = std::int64_t(pn.as_uint());
                if (cmp <= highest_sent || issue_packet_number < cmp) {
                    return unexpect(error::Error{
                        "packet number is not valid in order. use status.next_packet_number() to get next packet number",
                        error::Category::lib, error::fnet_quic_packet_number_error});
                }
                const auto range_begin = highest_sent + 1;
                highest_sent = pn.as_uint();
                if (status.is_ack_eliciting()) {
                    last_ack_eliciting_packet_sent_time_ = config.clock.now();
                }
                if (status.is_ack_eliciting() && status.is_byte_counted()) {
                    ack_eliciting_in_flight_sent_packet_count++;
                }
                return std::pair{range_begin, pn.as_uint()};
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
            packetnum::Value largest_recv_packet = 0;

           public:
            constexpr void reset() {
                largest_recv_packet = 0;
            }

            constexpr void on_packet_processed(packetnum::Value pn) {
                if (largest_recv_packet < pn) {
                    largest_recv_packet = pn;
                }
            }

            constexpr packetnum::Value largest_received_packet_number() const {
                return largest_recv_packet;
            }
        };

        struct SentAckTracker {
           private:
            packetnum::Value largest_one_rtt_acked_sent_ack = 0;

           public:
            constexpr void reset() {
                largest_one_rtt_acked_sent_ack = 0;
            }

            constexpr packetnum::Value get_onertt_largest_acked_sent_ack() const {
                return largest_one_rtt_acked_sent_ack;
            }

            constexpr void on_packet_acked(PacketNumberSpace space, packetnum::Value largest_sent_ack) {
                if (space == PacketNumberSpace::application) {
                    if (largest_sent_ack != packetnum::infinity) {
                        largest_one_rtt_acked_sent_ack = max_(largest_one_rtt_acked_sent_ack, largest_sent_ack + 1);
                    }
                }
            }
        };

    }  // namespace fnet::quic::status
}  // namespace futils

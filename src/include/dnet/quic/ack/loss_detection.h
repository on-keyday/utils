/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// loss_detection - loss detection handler for QUIC
#pragma once
#include "packet_history.h"
#include "congestion.h"
#include "idle_timer.h"
#include "controler_impl.h"
#include <algorithm>
#include <ranges>

namespace utils {
    namespace dnet {
        namespace quic::ack {

            constexpr auto errIdleTimeout = error::Error("IDLE_TIMEOUT");

            struct LossDetectionHandler {
                PacketHistories pn_spaces;
                time::Clock clock;
                RTT rtt;
                LossDetectParams loss_detect_params;
                PTOParams pto;
                time::Timer loss_detect_timer;
                LossDetectFlags flags;
                Congestion cong;
                IdleTimer idle_timer;
                TokenBudgetPacer pacer;

                void init(time::time_t inirtt_millisec,
                          std::uint64_t max_udp_payload_size) {
                    rtt.reset(clock, inirtt_millisec);
                    cong.init(max_udp_payload_size);
                }

                bool can_send(PacketNumberSpace space) {
                    return pacer.can_send(clock) || pto.is_probe_required(space);
                }

                error::Error on_packet_sent(PacketNumberSpace space, SentPacket&& sent) {
                    const auto in_flight = sent.in_flight;
                    const auto sent_bytes = sent.sent_bytes;
                    const auto ack_eliciting = sent.ack_eliciting;
                    const auto time_sent = clock.now();
                    if (!time_sent.valid()) {
                        return QUICError{
                            .msg = "clock.now is not valid. clock is broken",
                        };
                    }
                    sent.time_sent = time_sent;
                    auto& pn_space = pn_spaces[space_to_index(space)];
                    if (!pn_space.on_sent_packet(std::move(sent))) {
                        return QUICError{
                            .msg = "failed to add sent packet. maybe already added",
                        };
                    }
                    flags.on_packet_sent(sent_bytes, space);
                    pto.on_packet_sent(ack_eliciting);
                    idle_timer.on_packet_sent(time_sent, ack_eliciting);
                    pacer.on_packet_sent(cong, rtt, clock, time_sent, sent_bytes);
                    if (in_flight) {
                        cong.on_packet_sent(sent_bytes);
                        if (auto err = pn_spaces.set_loss_detection_timer(loss_detect_timer, pto, clock, rtt, flags)) {
                            return err;
                        }
                    }
                    return error::none;
                }

                error::Error on_packet_recived(PacketNumberSpace space, std::uint64_t recv_size) {
                    idle_timer.on_packet_recieved(clock.now());
                    auto was_limit = flags.is_at_anti_amplification_limit();
                    flags.on_packet_received(recv_size, space);
                    if (was_limit && flags.is_at_anti_amplification_limit()) {
                        auto err = pn_spaces.set_loss_detection_timer(loss_detect_timer, pto, clock, rtt, flags);
                        if (err) {
                            return err;
                        }
                        if (loss_detect_timer.timeout(clock)) {
                            return on_loss_detection_timeout();
                        }
                    }
                    return error::none;
                }

                error::Error on_ack_received(frame::ACKFrame<slib::vector>& ack, PacketNumberSpace space) {
                    flags.on_ack_received(space);
                    auto& pn_space = pn_spaces[space_to_index(space)];
                    auto largest_pn = packetnum::Value(ack.largest_ack.value);
                    if (pn_space.largest_acked_packet == packetnum::infinity) {
                        pn_space.largest_acked_packet = largest_pn;
                    }
                    else {
                        pn_space.largest_acked_packet = max_(
                            pn_space.largest_acked_packet,
                            largest_pn);
                    }
                    RemovedPackets acked_packets;
                    bool incl_ack_eliciting = false;
                    auto err = pn_space.detect_and_remove_acked_packets(acked_packets, incl_ack_eliciting, ack);
                    if (err) {
                        return err;
                    }
                    if (acked_packets.empty()) {
                        return error::none;
                    }
                    std::sort(acked_packets.begin(), acked_packets.end(), [](auto&& a, auto&& b) { return a.packet_number > b.packet_number; });
                    // std::ranges::sort(acked_packets, std::greater{}, [](auto& a) { return a.packet_number; });
                    auto& largest = acked_packets[0];
                    if (largest.packet_number == largest_pn && incl_ack_eliciting) {
                        if (!rtt.sample_rtt(clock, largest.time_sent, frame::decode_ack_delay(ack.ack_delay.value, rtt.ack_delay_exponent))) {
                            return QUICError{
                                .msg = "sample_rtt calculation failed. maybe clock is broken",
                                .frame_type = FrameType::ACK,
                            };
                        }
                    }
                    if (ack.type.type_detail() == FrameType::ACK_ECN) {
                        if (auto err = process_ecn(ack, space)) {
                            return err;
                        }
                    }
                    RemovedPackets lost_packets;
                    err = pn_space.detect_and_remove_lost_packets(lost_packets, clock, rtt, loss_detect_params);
                    if (err) {
                        return err;
                    }
                    if (!lost_packets.empty()) {
                        err = cong.on_packets_lost(clock, rtt, lost_packets);
                        if (err) {
                            return err;
                        }
                    }
                    err = cong.on_packets_ack(acked_packets);
                    if (err) {
                        return err;
                    }
                    pto.on_ack_recved(flags);
                    return pn_spaces.set_loss_detection_timer(loss_detect_timer, pto, clock, rtt, flags);
                }

                error::Error process_ecn(frame::ACKFrame<slib::vector>& ack, PacketNumberSpace space) {
                    auto& pn_space = pn_spaces[space_to_index(space)];
                    pn_space.ecn = ack.ecn_counts;
                    auto sent = pn_space.sent_packets[packetnum::Value(ack.largest_ack.value)].time_sent;
                    return cong.on_congestion_event(clock, sent);
                }

                error::Error maybeTimeout() {
                    if (loss_detect_timer.timeout(clock)) {
                        return on_loss_detection_timeout();
                    }
                    if (idle_timer.is_idle_timeout(clock)) {
                        return errIdleTimeout;
                    }
                    return error::none;
                }

                void set_handshake_confirmed() {
                    flags.handshake_confirmed = true;
                    idle_timer.handshake_confirmed = true;
                }

                void apply_transport_param(std::uint64_t peer_idle_timeout,
                                           std::uint64_t local_idle_timeout,
                                           std::uint64_t max_ack_delay,
                                           std::uint64_t ack_delay_exponent) {
                    if (peer_idle_timeout == 0) {
                        idle_timer.idle_timeout = local_idle_timeout;
                    }
                    else if (local_idle_timeout == 0) {
                        idle_timer.idle_timeout = peer_idle_timeout;
                    }
                    else {
                        idle_timer.idle_timeout = min_(local_idle_timeout, peer_idle_timeout);
                    }
                    idle_timer.idle_timeout = clock.to_clock_granurarity(idle_timer.idle_timeout);
                    rtt.max_ack_delay = clock.to_clock_granurarity(max_ack_delay);
                    rtt.ack_delay_exponent = ack_delay_exponent;
                }

                error::Error on_loss_detection_timeout() {
                    auto [earliest_loss_time, space] = pn_spaces.get_loss_time_and_space();
                    if (earliest_loss_time != 0) {
                        RemovedPackets losts;
                        auto err = pn_spaces[space_to_index(space)].detect_and_remove_lost_packets(losts, clock, rtt, loss_detect_params);
                        if (err) {
                            return err;
                        }
                        if (losts.empty()) {
                            return QUICError{
                                .msg = "loss_detection_timeout but lost packet not found",
                            };
                        }
                        if (auto err = cong.on_packets_lost(clock, rtt, losts)) {
                            return err;
                        }
                        return pn_spaces.set_loss_detection_timer(loss_detect_timer, pto, clock, rtt, flags);
                    }
                    if (pn_spaces.no_ack_eliciting_in_flight() &&
                        !flags.peer_completed_address_validation()) {
                        if (flags.has_sent_handshake) {
                            pto.on_pto_no_flight(PacketNumberSpace::handshake);
                        }
                        else {
                            pto.on_pto_no_flight(PacketNumberSpace::initial);
                        }
                    }
                    else {
                        auto [t, space] = pn_spaces.get_pto_and_space(pto, clock, rtt, flags);
                        if (!t.valid()) {
                            return error::none;  // TODO(on-keyday): is this ok?
                        }
                        pto.on_pto_timeout(space);
                    }
                    return pn_spaces.set_loss_detection_timer(loss_detect_timer, pto, clock, rtt, flags);
                }

                error::Error on_packet_number_space_discarded(PacketNumberSpace space) {
                    return pn_spaces.on_packet_number_space_discarded(cong, space, loss_detect_timer, pto, clock, rtt, flags);
                }

                // returns (pn,largest_acked_pn)
                std::pair<packetnum::Value, std::uint64_t> next_packet_number(PacketNumberSpace space) {
                    auto& pn_space = pn_spaces[space_to_index(space)];
                    return {
                        pn_space.issue_packet_number,
                        pn_space.largest_acked_packet,
                    };
                }

                void consume_packet_number(PacketNumberSpace space) {
                    pn_spaces[space_to_index(space)].issue_packet_number++;
                }

                std::uint64_t get_largest_pn(PacketNumberSpace space) {
                    return pn_spaces[space_to_index(space)].largest_recv_packet;
                }

                void update_higest_recv_packet(packetnum::Value pn, PacketNumberSpace space) {
                    auto& high = pn_spaces[space_to_index(space)].largest_recv_packet;
                    if (high < pn) {
                        high = pn;
                    }
                }
            };
        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

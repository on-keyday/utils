/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// loss_detection - loss detection handler for QUIC
#pragma once
#include "packet_history.h"
#include "congestion.h"
#include <algorithm>
#include <ranges>

namespace utils {
    namespace dnet {
        namespace quic::ack {

            struct LossEventHandlers {
                closure::Closure<error::Error> sendOneACKElicitingHandshakePacket;
                closure::Closure<error::Error> sendOneACKElicitingPaddedInitialPacket;
                closure::Closure<error::Error, PacketNumberSpace> sendOneOrTwoACKElicitingPackets;
            };

            struct LossDetectionHandler {
                PacketHistories pn_spaces;
                Clock clock;
                RTT rtt;
                LossDetectParam params;
                LossDetectTimer timer;
                LossDetectFlags flags;
                Congestion cong;
                LossEventHandlers handlers;

                error::Error on_packet_sent(PacketNumberSpace space, SentPacket&& sent) {
                    auto in_flight = sent.in_flight;
                    auto sent_bytes = sent.sent_bytes;
                    auto time_sent = clock.now();
                    if (time_sent == invalid_time) {
                        return QUICError{
                            .msg = "clock.now is not valid. that is clock is broken",

                        };
                    }
                    sent.time_sent = time_sent;
                    auto& pn_space = pn_spaces[int(space)];
                    if (!pn_space.on_sent_packet(std::move(sent))) {
                        return QUICError{
                            .msg = "failed to add sent packet. maybe already added",

                        };
                    }
                    if (in_flight) {
                        cong.on_packet_sent(sent_bytes);
                    }
                    return error::none;
                }

                error::Error on_datagram_recived() {
                    if (flags.server_is_at_anti_amplification_limit) {
                        auto err = pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                        if (err) {
                            return err;
                        }
                        if (timer.timeout < clock.now()) {
                            return on_loss_detection_timeout();
                        }
                    }
                    return error::none;
                }

                error::Error on_ack_received(frame::ACKFrame& ack, PacketNumberSpace space) {
                    auto& pn_space = pn_spaces[int(space)];
                    auto largest_pn = PacketNumber(ack.largest_ack.value);
                    if (pn_space.largest_acked_packet == PacketNumber::infinite) {
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
                        if (!rtt.sample_rtt(clock, largest.time_sent, ack.ack_delay.value)) {
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
                    err = pn_space.detect_and_remove_lost_packets(lost_packets, clock, rtt, params);
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
                    if (flags.peer_completed_address_validation()) {
                        params.pto_count = 0;
                    }
                    return pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                }

                error::Error process_ecn(frame::ACKFrame& ack, PacketNumberSpace space) {
                    error::Error err;
                    auto res = frame::ACKFrame::parse_ecn_counts(ack.ecn_counts, [&](auto ect0, auto ect1, auto ce) {
                        auto& pn_space = pn_spaces[int(space)];
                        pn_space.ecn = frame::ECNCounts{ect0, ect1, ce};
                        auto sent = pn_space.sent_packets[PacketNumber(ack.largest_ack.value)].time_sent;
                        err = cong.on_congestion_event(clock, sent);
                        return true;
                    });
                    if (res == 0) {
                        return QUICError{
                            .msg = "ecn counts encoding is not valid",
                            .transport_error = TransportError::FRAME_ENCODING_ERROR,
                            .frame_type = ack.type.type_detail(),
                            .base = std::move(err),
                        };
                    }
                    return err;
                }

                error::Error maybeTimeout() {
                    if (timer.timeout != -1 && timer.timeout < clock.now()) {
                        return on_loss_detection_timeout();
                    }
                    return error::none;
                }

                error::Error on_loss_detection_timeout() {
                    auto [earliest_loss_time, space] = pn_spaces.get_loss_time_and_space();
                    if (earliest_loss_time != 0) {
                        RemovedPackets losts;
                        auto err = pn_spaces[int(space)].detect_and_remove_lost_packets(losts, clock, rtt, params);
                        if (err) {
                            return err;
                        }
                        if (losts.empty()) {
                            return QUICError{
                                .msg = "loss_detection_timeout but lost packet not found",

                            };
                        }
                        // on_packets_lost
                        return pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                    }
                    if (pn_spaces.no_ack_eliciting_in_flight()) {
                        if (flags.peer_completed_address_validation()) {
                            return QUICError{
                                .msg = "no ack eliciting in flight but peer completed address validation",

                            };
                        }
                        if (flags.has_handshake_keys) {
                            if (handlers.sendOneACKElicitingHandshakePacket) {
                                auto err = handlers.sendOneACKElicitingHandshakePacket();
                                if (err) {
                                    return err;
                                }
                            }
                        }
                        else {
                            if (handlers.sendOneACKElicitingPaddedInitialPacket) {
                                auto err = handlers.sendOneACKElicitingPaddedInitialPacket();
                                if (err) {
                                    return err;
                                }
                            }
                        }
                    }
                    else {
                        if (handlers.sendOneOrTwoACKElicitingPackets) {
                            auto [_, space] = pn_spaces.get_pto_and_space(params, clock, rtt, flags);
                            auto err = handlers.sendOneOrTwoACKElicitingPackets(space);
                            if (err) {
                                return err;
                            }
                        }
                    }
                    params.pto_count++;
                    return pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                }

                error::Error on_packet_number_space_discarded(PacketNumberSpace space) {
                    return pn_spaces.on_packet_number_space_discarded(cong, space, timer, params, clock, rtt, flags);
                }

                QPacketNumber encode_packet_number(size_t pn, PacketNumberSpace space) {
                    auto largest_ack = pn_spaces[int(space)].largest_acked_packet;
                    size_t num_unacked = 0;
                    if (largest_ack == PacketNumber::infinite) {
                        num_unacked = pn + 1;
                    }
                    else {
                        num_unacked = pn - size_t(largest_ack);
                    }
                    auto min_bits = log2i(num_unacked) + 1;
                    auto min_bytes = min_bits / 8 + (min_bits % 8 ? 1 : 0);
                    if (min_bytes == 1) {
                        return {std::uint32_t(pn & 0xff), 1};
                    }
                    if (min_bytes == 2) {
                        return {std::uint32_t(pn & 0xffff), 2};
                    }
                    if (min_bytes == 3) {
                        return {std::uint32_t(pn & 0xffffff), 3};
                    }
                    if (min_bytes == 4) {
                        return {std::uint32_t(pn & 0xffffffff), 4};
                    }
                    return {};  // is this occurer?
                }

                std::pair<QPacketNumber, size_t> next_packet_number(PacketNumberSpace space) {
                    auto& should_issue = pn_spaces[int(space)].issue_packet_number;
                    auto yielded = encode_packet_number(should_issue, space);
                    return {yielded, should_issue};
                }

                void consume_packet_number(PacketNumberSpace space) {
                    pn_spaces[int(space)].issue_packet_number++;
                }

                PacketNumber decode_packet_number(QPacketNumber pn, PacketNumberSpace space) {
                    auto& largest_pn = pn_spaces[int(space)].largest_recv_packet;
                    auto expected_pn = largest_pn + 1;
                    auto pn_win = 1 << (pn.len * 8);
                    auto half_win = pn_win >> 1;
                    auto pn_mask = pn_win - 1;
                    auto base = (size_t(expected_pn) & ~pn_mask);
                    size_t next = base + pn_win;
                    size_t prev = base < pn_win ? 0 : base - pn_win;
                    auto delta = [](size_t a, size_t b) {
                        if (a < b) {
                            return b - a;
                        }
                        return a - b;
                    };
                    auto closer = [&](size_t t, size_t a, size_t b) {
                        if (delta(t, a) < delta(t, b)) {
                            return a;
                        }
                        return b;
                    };
                    auto selected = closer(expected_pn, base + pn.value, closer(expected_pn, prev + pn.value, next + pn.value));
                    return PacketNumber(selected);
                }

                void update_higest_recv_packet(PacketNumber pn, PacketNumberSpace space) {
                    auto& high = pn_spaces[int(space)].largest_recv_packet;
                    if (high < size_t(pn)) {
                        high = size_t(pn);
                    }
                }
            };
        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

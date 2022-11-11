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
                closure::Closure<TransportError> sendOneACKElicitingHandshakePacket;
                closure::Closure<TransportError> sendOneACKElicitingPaddedInitialPacket;
                closure::Closure<TransportError, PacketNumberSpace> sendOneOrTwoACKElicitingPackets;
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

                TransportError on_packet_sent(PacketNumberSpace space, SentPacket&& sent) {
                    auto in_flight = sent.in_flight;
                    auto sent_bytes = sent.sent_bytes;
                    auto time_sent = clock.now();
                    sent.time_sent = time_sent;
                    auto& pn_space = pn_spaces[int(space)];
                    if (!pn_space.on_sent_packet(std::move(sent))) {
                        return TransportError::INTERNAL_ERROR;
                    }
                    if (in_flight) {
                        cong.on_packet_sent(sent_bytes);
                    }
                    return TransportError::NO_ERROR;
                }

                TransportError on_datagram_recived() {
                    if (flags.server_is_at_anti_amplification_limit) {
                        auto err = pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                        if (err != TransportError::NO_ERROR) {
                            return err;
                        }
                        if (timer.timeout < clock.now()) {
                            return on_loss_detection_timeout();
                        }
                    }
                    return TransportError::NO_ERROR;
                }

                TransportError on_ack_received(frame::ACKFrame& ack, PacketNumberSpace space) {
                    auto& pn_space = pn_spaces[int(space)];
                    auto largest_pn = PacketNumber(ack.largest_ack.qvarint());
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
                    if (err != TransportError::NO_ERROR) {
                        return err;
                    }
                    if (acked_packets.empty()) {
                        return TransportError::NO_ERROR;
                    }
                    std::sort(acked_packets.begin(), acked_packets.end(), [](auto&& a, auto&& b) { return a.packet_number > b.packet_number; });
                    // std::ranges::sort(acked_packets, std::greater{}, [](auto& a) { return a.packet_number; });
                    auto& largest = acked_packets[0];
                    if (largest.packet_number == largest_pn && incl_ack_eliciting) {
                        if (!rtt.sample_rtt(clock, largest.time_sent, ack.ack_delay.qvarint())) {
                            return TransportError::INTERNAL_ERROR;
                        }
                    }
                    if (ack.type.type_detail() == FrameType::ACK_ECN) {
                    }
                    RemovedPackets lost_packets;
                    err = pn_space.detect_and_remove_lost_packets(lost_packets, clock, rtt, params);
                    if (err != TransportError::NO_ERROR) {
                        return err;
                    }
                    if (!lost_packets.empty()) {
                        err = cong.on_packets_lost(clock, rtt, lost_packets);
                        if (err != TransportError::NO_ERROR) {
                            return err;
                        }
                    }
                    err = cong.on_packets_ack(acked_packets);
                    if (err != TransportError::NO_ERROR) {
                        return err;
                    }
                    if (flags.peer_completed_address_validation()) {
                        params.pto_count = 0;
                    }
                    return pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                }

                TransportError process_ecn(frame::ACKFrame& ack, PacketNumberSpace space) {
                    TransportError err = TransportError::NO_ERROR;
                    auto res = frame::ACKFrame::parse_ecn_counts(ack.ecn_counts, [&](auto ect0, auto ect1, auto ce) {
                        auto& pn_space = pn_spaces[int(space)];
                        pn_space.ecn = frame::ECNCounts{ect0, ect1, ce};
                        auto sent = pn_space.sent_packets[PacketNumber(ack.largest_ack.qvarint())].time_sent;
                        err = cong.on_congestion_event(clock, sent);
                        return true;
                    });
                    if (res == 0) {
                        return TransportError::FRAME_ENCODING_ERROR;
                    }
                    return err;
                }

                TransportError maybeTimeout() {
                    if (timer.timeout != -1 && timer.timeout < clock.now()) {
                        return on_loss_detection_timeout();
                    }
                    return TransportError::NO_ERROR;
                }

                TransportError on_loss_detection_timeout() {
                    auto [earliest_loss_time, space] = pn_spaces.get_loss_time_and_space();
                    if (earliest_loss_time != 0) {
                        RemovedPackets losts;
                        auto err = pn_spaces[int(space)].detect_and_remove_lost_packets(losts, clock, rtt, params);
                        if (err != TransportError::NO_ERROR) {
                            return err;
                        }
                        if (losts.empty()) {
                            return TransportError::INTERNAL_ERROR;
                        }
                        // on_packets_lost
                        return pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                    }
                    if (pn_spaces.no_ack_eliciting_in_flight()) {
                        if (flags.peer_completed_address_validation()) {
                            return TransportError::INTERNAL_ERROR;
                        }
                        if (flags.has_handshake_keys) {
                            if (handlers.sendOneACKElicitingHandshakePacket) {
                                auto err = handlers.sendOneACKElicitingHandshakePacket();
                                if (err != TransportError::NO_ERROR) {
                                    return err;
                                }
                            }
                        }
                        else {
                            if (handlers.sendOneACKElicitingPaddedInitialPacket) {
                                auto err = handlers.sendOneACKElicitingPaddedInitialPacket();
                                if (err != TransportError::NO_ERROR) {
                                    return err;
                                }
                            }
                        }
                    }
                    else {
                        if (handlers.sendOneOrTwoACKElicitingPackets) {
                            auto [_, space] = pn_spaces.get_pto_and_space(params, clock, rtt, flags);
                            auto err = handlers.sendOneOrTwoACKElicitingPackets(space);
                            if (err != TransportError::NO_ERROR) {
                                return err;
                            }
                        }
                    }
                    params.pto_count++;
                    return pn_spaces.set_loss_detection_timer(timer, params, clock, rtt, flags);
                }

                TransportError on_packet_number_space_discarded(PacketNumberSpace space) {
                    return pn_spaces.on_packet_number_space_discarded(cong, space, timer, params, clock, rtt, flags);
                }
            };
        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

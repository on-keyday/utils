/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_history - sent packet histories
#pragma once
#include "rtt.h"
#include "sent_packet.h"
#include "../../dll/allocator.h"
#include <unordered_map>
#include "../frame/frame.h"
#include "../transport_error.h"
#include "../../../helper/condincr.h"
#include "congestion.h"

namespace utils {
    namespace dnet {
        namespace quic::ack {

            struct Ratio {
                time_t num = 0;
                time_t den = 0;
            };

            struct LossDetectParam {
                size_t packet_order_threshold = 3;
                Ratio time_threshold_ratio{9, 8};
                size_t pto_count = 0;

                constexpr time_t loss_delay(Clock& clock, RTT& rtt) const {
                    if (time_threshold_ratio.den == 0) {
                        return -1;
                    }
                    // HACK(on-keyday): recommended is 9/8 so optimize for den == 8
                    auto k = (time_threshold_ratio.num * min_(rtt.smoothed_rtt, rtt.latest_rtt));
                    if (time_threshold_ratio.den == 8) {
                        auto candidate = k >> 3;
                        if (candidate < 0) {
                            return -1;  // BUG overflow
                        }
                        return max_(candidate, clock.granularity);
                    }
                    auto candidate = k / time_threshold_ratio.den;
                    if (candidate < 0) {
                        return -1;  // BUG overflow
                    }
                    return max_(candidate, clock.granularity);
                }

                constexpr time_t probe_timeout_duration(Clock& clock, RTT& rtt) const {
                    return rtt.smoothed_rtt + max_(rtt.rttvar << 2, clock.granularity) * (utime_t(1) << pto_count);
                }
            };

            struct LossDetectFlags {
                bool has_handshake_keys = false;
                bool handshake_done = false;
                bool server_is_at_anti_amplification_limit = false;
                bool is_server = false;
                bool has_recived_handshake_ack = false;

                bool peer_completed_address_validation() const {
                    if (is_server) {
                        return true;
                    }
                    return has_recived_handshake_ack || handshake_done;
                }
            };

            struct PacketSpaceHistory {
                PacketNumber largest_acked_packet = PacketNumber::infinite;
                time_t last_ack_eliciting_packet_time = 0;
                time_t loss_time = 0;
                using Packets =
                    std::unordered_map<
                        PacketNumber, SentPacket,
                        std::hash<PacketNumber>, std::equal_to<PacketNumber>,
                        glheap_objpool_allocator<std::pair<const PacketNumber, SentPacket>>>;
                Packets sent_packets;
                frame::ECNCounts ecn;
                size_t ack_eliciting_in_flight = 0;

                bool on_sent_packet(SentPacket&& sent) {
                    auto time_sent = sent.time_sent;
                    auto is_ack_eliciting_in_flight = sent.in_flight && sent.ack_eliciting;
                    if (!sent_packets.emplace(sent.packet_number, std::move(sent)).second) {
                        return false;
                    }
                    if (is_ack_eliciting_in_flight) {
                        last_ack_eliciting_packet_time = sent.time_sent;
                        ack_eliciting_in_flight++;
                    }
                    return true;
                }

                TransportError detect_and_remove_acked_packets(RemovedPackets& rem, bool& incl_ack_eliciting, const frame::ACKFrame& ack) {
                    incl_ack_eliciting = false;
                    std::vector<frame::ACKRange, glheap_allocator<frame::ACKRange>> ranges;
                    if (!frame::get_ackranges(ranges, ack)) {
                        return TransportError::FRAME_ENCODING_ERROR;
                    }
                    bool removed = false;
                    auto incr = helper::cond_incr(removed);
                    auto largest = ranges[0].largest;
                    auto smallest = ranges[ranges.size() - 1].smallest;
                    for (auto it = sent_packets.begin(); it != sent_packets.end(); incr(it)) {
                        auto& sent = it->second;
                        auto pn = sent.packet_number;
                        if (pn < smallest ||
                            largest < pn) {
                            continue;
                        }
                        const auto rsize = ranges.size();
                        if (rsize > 1) {
                            frame::ACKRange range;
                            for (size_t i = 0; i < rsize; i++) {
                                auto& r = ranges[rsize - 1 - i];
                                if (pn < r.largest) {
                                    range = r;
                                    break;
                                }
                            }
                            if (pn < range.smallest) {
                                continue;
                            }
                            if (pn > range.largest) {
                                return TransportError::INTERNAL_ERROR;
                            }
                        }
                        if (sent.ack_eliciting && sent.in_flight) {
                            ack_eliciting_in_flight--;
                        }
                        incl_ack_eliciting = incl_ack_eliciting || sent.ack_eliciting;
                        rem.push_back(std::move(sent));
                        it = sent_packets.erase(it);
                        removed = true;
                    }
                    return TransportError::NO_ERROR;
                }

                TransportError detect_and_remove_lost_packets(RemovedPackets& lost_packets, Clock& clock, RTT& rtt, LossDetectParam& param) {
                    if (largest_acked_packet == PacketNumber::infinite) {
                        return TransportError::INTERNAL_ERROR;
                    }
                    loss_time = 0;
                    auto loss_delay = param.loss_delay(clock, rtt);
                    if (loss_delay < 0) {
                        return TransportError::INTERNAL_ERROR;
                    }
                    auto lost_send_time = clock.now() - loss_delay;
                    bool removed = false;
                    auto incr = helper::cond_incr(removed);
                    for (auto it = sent_packets.begin(); it != sent_packets.end(); incr(it)) {
                        auto& unacked = it->second;
                        if (unacked.packet_number > largest_acked_packet) {
                            continue;
                        }
                        if (unacked.time_sent <= lost_send_time ||
                            largest_acked_packet >= unacked.packet_number + param.packet_order_threshold) {
                            if (unacked.ack_eliciting && unacked.in_flight) {
                                ack_eliciting_in_flight--;
                            }
                            lost_packets.push_back(std::move(unacked));
                            it = sent_packets.erase(it);
                            removed = true;
                        }
                        else {
                            if (loss_time == 0) {
                                loss_time = unacked.time_sent;
                            }
                            else {
                                loss_time = min_(loss_time, unacked.time_sent);
                            }
                        }
                    }
                    return TransportError::NO_ERROR;
                }
            };

            struct LossDetectTimer {
                time_t timeout = -1;
                void update(time_t t) {
                    timeout = t;
                }

                void cancel() {
                    timeout = -1;
                }
            };

            struct PacketHistories {
                PacketSpaceHistory pn_spaces[3];

                constexpr bool no_ack_eliciting_in_flight() const {
                    return pn_spaces[0].ack_eliciting_in_flight == 0 &&
                           pn_spaces[1].ack_eliciting_in_flight == 0 &&
                           pn_spaces[2].ack_eliciting_in_flight == 0;
                }

                constexpr auto& operator[](size_t i) {
                    return pn_spaces[i];
                }

                std::pair<time_t, PacketNumberSpace> get_loss_time_and_space() {
                    PacketNumberSpace space;
                    time_t time = 0;
                    for (auto i = 0; i < 3; i++) {
                        if (time == 0 || pn_spaces[i].loss_time < time) {
                            time = pn_spaces[i].loss_time;
                            space = PacketNumberSpace(i);
                        }
                    }
                    return {time, space};
                }

                std::pair<time_t, PacketNumberSpace> get_pto_and_space(LossDetectParam& param, Clock& clock, RTT& rtt, LossDetectFlags& flags) {
                    auto duration = param.probe_timeout_duration(clock, rtt);
                    if (duration < 0) {
                        return {-1, PacketNumberSpace::initial};
                    }
                    if (no_ack_eliciting_in_flight()) {
                        if (flags.peer_completed_address_validation()) {
                            return {-1, PacketNumberSpace::initial};
                        }
                        if (flags.has_handshake_keys) {
                            return {clock.now() + duration, PacketNumberSpace::handshake};
                        }
                        else {
                            return {clock.now() + duration, PacketNumberSpace::initial};
                        }
                    }
                    time_t pto_timeout = -1;
                    PacketNumberSpace pto_space = PacketNumberSpace::initial;
                    for (auto i = 0; i < 3; i++) {
                        auto& pn_space = pn_spaces[i];
                        if (pn_space.ack_eliciting_in_flight == 0) {
                            continue;
                        }
                        if (i == 2) {
                            if (!flags.handshake_done) {
                                return {pto_timeout, pto_space};
                            }
                            duration += rtt.max_ack_delay * (utime_t(1) << param.pto_count);
                        }
                        auto t = pn_space.last_ack_eliciting_packet_time + duration;
                        if (pto_timeout == -1 || t < pto_timeout) {
                            pto_timeout = t;
                            pto_space = PacketNumberSpace(i);
                        }
                    }
                    return {pto_timeout, pto_space};
                }

                TransportError set_loss_detection_timer(
                    LossDetectTimer& timer, LossDetectParam& param, Clock& clock, RTT& rtt, LossDetectFlags& flags) {
                    auto [earliest_loss_time, _] = get_loss_time_and_space();
                    if (earliest_loss_time != 0) {
                        timer.update(earliest_loss_time);
                        return TransportError::NO_ERROR;
                    }
                    if (flags.server_is_at_anti_amplification_limit) {
                        timer.cancel();
                        return TransportError::NO_ERROR;
                    }
                    if (no_ack_eliciting_in_flight() &&
                        flags.peer_completed_address_validation()) {
                        timer.cancel();
                    }
                    auto [timeout, _2] = get_pto_and_space(param, clock, rtt, flags);
                    if (timeout == -1) {
                        return TransportError::INTERNAL_ERROR;
                    }
                    timer.update(timeout);
                    return TransportError::NO_ERROR;
                }

                TransportError on_packet_number_space_discarded(
                    Congestion& cong, PacketNumberSpace space,
                    LossDetectTimer& timer, LossDetectParam& param, Clock& clock, RTT& rtt, LossDetectFlags& flags) {
                    if (space != PacketNumberSpace::initial && space != PacketNumberSpace::handshake) {
                        return TransportError::INTERNAL_ERROR;
                    }
                    auto& pn_space = pn_spaces[int(space)];
                    for (auto& p : pn_space.sent_packets) {
                        if (p.second.in_flight) {
                            cong.bytes_in_flight -= p.second.sent_bytes;
                        }
                    }
                    pn_space.sent_packets.clear();
                    pn_space.last_ack_eliciting_packet_time = 0;
                    pn_space.loss_time = 0;
                    return set_loss_detection_timer(timer, param, clock, rtt, flags);
                }
            };
        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

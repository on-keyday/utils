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
#include "../frame/ack.h"
#include "../transport_error.h"
#include "../../../helper/condincr.h"
#include "congestion.h"
#include "../../error.h"
#include "../../std/hash_map.h"
#include "pto_loss_param.h"

namespace utils {
    namespace dnet::quic::ack {

        using ACKRangeVec = slib::vector<frame::ACKRange>;

        struct PnRangeError {
            size_t pn = 0;
            size_t begin = 0;
            size_t end = 0;
            void error(auto&& pb) {
                helper::append(pb, "pn=");
                number::to_string(pb, pn);
                helper::append(pb, "begin=");
                number::to_string(pb, begin);
                helper::append(pb, " end=");
                number::to_string(pb, end);
            }
        };

        struct PacketSpaceHistory {
            std::uint64_t issue_packet_number = 0;
            std::uint64_t highest_sent = 0;
            packetnum::Value largest_acked_packet = packetnum::infinity;
            time::Time last_ack_eliciting_packet_time = 0;
            time::Time loss_time = 0;
            slib::hash_map<packetnum::Value, SentPacket> sent_packets;
            frame::ECNCounts ecn;
            std::uint64_t ack_eliciting_in_flight = 0;
            std::uint64_t largest_recv_packet = 0;

            bool on_sent_packet(SentPacket&& sent) {
                auto time_sent = sent.time_sent;
                auto is_ack_eliciting_in_flight = sent.in_flight && sent.ack_eliciting;
                auto sent_number = size_t(sent.packet_number);
                if (!sent_packets.try_emplace(sent.packet_number, std::move(sent)).second) {
                    return false;
                }
                for (auto i = highest_sent + 1; i < sent_number; i++) {
                    sent_packets.emplace(
                        packetnum::Value(i),
                        SentPacket{
                            .packet_number = packetnum::Value(i),
                            .skiped = true,
                            .time_sent = time_sent,
                        });
                }
                highest_sent = sent_number;
                if (is_ack_eliciting_in_flight) {
                    last_ack_eliciting_packet_time = sent.time_sent;
                    ack_eliciting_in_flight++;
                }
                return true;
            }

            error::Error detect_and_remove_acked_packets(RemovedPackets& rem, bool& incl_ack_eliciting, const frame::ACKFrame<slib::vector>& ack) {
                incl_ack_eliciting = false;
                auto [ranges, ok] = frame::convert_from_ACKFrame(ack);
                if (!ok) {
                    return QUICError{
                        .msg = "ACK range validation failed",
                        .transport_error = TransportError::FRAME_ENCODING_ERROR,
                        .frame_type = ack.type.type_detail(),
                    };
                }
                bool removed = false;
                auto incr = helper::cond_incr(removed);
                auto largest = ranges[0].largest;
                auto smallest = ranges[ranges.size() - 1].smallest;
                const auto rsize = ranges.size();
                for (auto it = sent_packets.begin(); it != sent_packets.end(); incr(it)) {
                    auto& sent = it->second;
                    auto pn = sent.packet_number;
                    if (pn < smallest ||
                        largest < pn) {
                        continue;
                    }
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
                            return QUICError{
                                .msg = "library bug!! range is invalid for pn",
                                .base = PnRangeError{.pn = size_t(pn), .begin = largest, .end = smallest},
                            };
                        }
                    }
                    if (sent.skiped) {
                        return QUICError{
                            .msg = "ack recived skipped packet.",
                            .transport_error = TransportError::PROTOCOL_VIOLATION,
                            .base = PnRangeError{.pn = size_t(pn)},
                        };
                    }
                    if (sent.ack_eliciting && sent.in_flight) {
                        ack_eliciting_in_flight--;
                    }
                    incl_ack_eliciting = incl_ack_eliciting || sent.ack_eliciting;
                    rem.push_back(std::move(sent));
                    it = sent_packets.erase(it);
                    removed = true;
                }
                return error::none;
            }

            error::Error detect_and_remove_lost_packets(RemovedPackets& lost_packets, time::Clock& clock, RTT& rtt, LossDetectParams& param) {
                if (largest_acked_packet == packetnum::infinity) {
                    return QUICError{
                        .msg = "largest_acked_packet is not set. nothing acked before loss detection.",
                    };
                }
                loss_time = 0;
                auto loss_delay = param.loss_delay(clock, rtt);
                if (!loss_delay.valid()) {
                    return QUICError{
                        .msg = "loss_delay calculation overflow or failure. calculation result is under 0",

                    };
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
                return error::none;
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

            std::pair<time::Time, PacketNumberSpace> get_loss_time_and_space() {
                PacketNumberSpace space;
                time::Time time = 0;
                for (auto i = 0; i < 3; i++) {
                    if (time == 0 || pn_spaces[i].loss_time < time) {
                        time = pn_spaces[i].loss_time;
                        space = PacketNumberSpace(i);
                    }
                }
                return {time, space};
            }

            std::pair<time::Time, PacketNumberSpace> get_pto_and_space(PTOParams& param, time::Clock& clock, RTT& rtt, LossDetectFlags& flags) {
                auto duration = param.probe_timeout_duration(clock, rtt);
                if (duration < 0) {
                    return {time::invalid, PacketNumberSpace::initial};
                }
                if (no_ack_eliciting_in_flight()) {
                    if (flags.peer_completed_address_validation()) {
                        return {time::invalid, PacketNumberSpace::initial};
                    }
                    if (flags.has_sent_handshake) {
                        return {clock.now() + duration, PacketNumberSpace::handshake};
                    }
                    else {
                        return {clock.now() + duration, PacketNumberSpace::initial};
                    }
                }
                time::Time pto_timeout = time::invalid;
                PacketNumberSpace pto_space = PacketNumberSpace::initial;
                for (auto i = 0; i < 3; i++) {
                    auto& pn_space = pn_spaces[i];
                    if (pn_space.ack_eliciting_in_flight == 0) {
                        continue;
                    }
                    if (i == space_to_index(PacketNumberSpace::application)) {
                        if (!flags.handshake_confirmed) {
                            return {pto_timeout, pto_space};
                        }
                        duration += rtt.max_ack_delay * (time::utime_t(1) << param.pto_count);
                    }
                    auto t = pn_space.last_ack_eliciting_packet_time + duration;
                    if (pto_timeout == -1 || t < pto_timeout) {
                        pto_timeout = t;
                        pto_space = PacketNumberSpace(i);
                    }
                }
                return {pto_timeout, pto_space};
            }

            error::Error set_loss_detection_timer(
                time::Timer& timer, PTOParams& param, time::Clock& clock, RTT& rtt, LossDetectFlags& flags) {
                auto [earliest_loss_time, _] = get_loss_time_and_space();
                if (earliest_loss_time != 0) {
                    timer.set_deadline(earliest_loss_time);
                    return error::none;
                }
                if (flags.is_at_anti_amplification_limit()) {
                    timer.cancel();
                    return error::none;
                }
                if (no_ack_eliciting_in_flight() &&
                    flags.peer_completed_address_validation()) {
                    timer.cancel();
                }
                auto [timeout, _2] = get_pto_and_space(param, clock, rtt, flags);
                if (timeout == -1) {
                    return QUICError{
                        .msg = "failed to calculate pto_timeout",

                    };
                }
                timer.set_deadline(timeout);
                return error::none;
            }

            error::Error on_packet_number_space_discarded(
                Congestion& cong, PacketNumberSpace space,
                time::Timer& timer, PTOParams& param, time::Clock& clock, RTT& rtt, LossDetectFlags& flags) {
                if (space != PacketNumberSpace::initial && space != PacketNumberSpace::handshake) {
                    return QUICError{
                        .msg = "invalid packet number space. need initial or handshake",
                    };
                }
                auto& pn_space = pn_spaces[space_to_index(space)];
                for (auto& p : pn_space.sent_packets) {
                    if (p.second.in_flight) {
                        cong.bytes_in_flight -= p.second.sent_bytes;
                    }
                    for (auto w : p.second.waiters) {
                        if (auto got = w.lock()) {
                            got->lost();
                        }
                    }
                }
                pn_space.sent_packets.clear();
                pn_space.last_ack_eliciting_packet_time = 0;
                pn_space.loss_time = 0;
                param.on_pn_space_discard();
                return set_loss_detection_timer(timer, param, clock, rtt, flags);
            }
        };

    }  // namespace dnet::quic::ack
}  // namespace utils

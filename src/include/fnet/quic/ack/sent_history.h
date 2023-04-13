/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_history - sent packet histories
#pragma once
#include "sent_packet.h"
#include "../../dll/allocator.h"
#include "../frame/ack.h"
#include "../transport_error.h"
#include "../../../helper/condincr.h"
#include "../../error.h"
#include "../../std/hash_map.h"
#include "../hash_fn.h"
#include "../status/status.h"
#include "../status/new_reno.h"

namespace utils {
    namespace fnet::quic::ack {

        struct SentPacketHistory {
            slib::hash_map<packetnum::Value, SentPacket> sent_packets[3];

            template <class Alg>
            error::Error on_packet_sent(status::Status<Alg>& status, status::PacketNumberSpace space, SentPacket&& sent) {
                sent.time_sent = status.now();
                auto [begin, end] = status.on_packet_sent(space, sent.packet_number, sent.sent_bytes, sent.time_sent, sent.status);
                if (begin == -1 && end == -1) {
                    return QUICError{
                        .msg = "failed to update QUIC status. maybe packet number is not valid in order. use status.next_packet_number() to get next packet number",
                    };
                }
                auto& sent_packet = sent_packets[space_to_index(space)];
                for (auto i = begin; i < end; i++) {
                    auto [_, ok] = sent_packet.try_emplace(
                        packetnum::Value(i),
                        SentPacket{
                            .skiped = true,
                            .time_sent = sent.time_sent,
                        });
                    if (!ok) {
                        return QUICError{
                            .msg = "failed to register skipped packet number. already registered?",
                        };
                    }
                }
                auto [_, ok] = sent_packet.try_emplace(sent.packet_number, std::move(sent));
                if (!ok) {
                    return QUICError{
                        .msg = "failed to register sent packet number. already registered?",
                    };
                }
                return error::none;
            }

            auto remove_lost_callback(error::Error& err, RemovedPackets* rem) {
                return [this, rem, &err](status::PacketNumberSpace space, auto&& is_lost, auto&& congestion_callback) {
                    auto& p = sent_packets[space_to_index(space)];
                    for (auto it = p.begin(); it != p.end();) {
                        SentPacket& sent = it->second;
                        // is_lost = LostReason(packetnum::Value,std::uint64_t sent_bytes,time::Time time_sent,packet::PacketStatus,bool is_mtu_probe)
                        if (auto res = is_lost(sent.packet_number, sent.sent_bytes, sent.time_sent, sent.status, sent.is_mtu_probe);
                            res != status::LostReason::not_lost) {
                            if (res == status::LostReason::invalid) {
                                err = QUICError{
                                    .msg = "internal state error (loss detection). library bug!!",
                                };
                                return false;
                            }
                            mark_as_lost(sent.waiters);
                            if (rem) {
                                rem->push_back(std::move(sent));
                            }
                            it = p.erase(it);
                            continue;
                        }
                        it++;
                    }
                    return true;
                    // congestion_callback is automatically called
                };
            }

            template <class Alg>
            error::Error on_ack_received(status::Status<Alg>& status,
                                         status::PacketNumberSpace space,
                                         RemovedPackets& acked, RemovedPackets* lost,
                                         const frame::ACKFrame<slib::vector>& ack,
                                         auto&& is_flow_control_limited) {
                auto [ranges, ok] = frame::convert_from_ACKFrame(ack);
                if (!ok) {
                    return QUICError{
                        .msg = "ACK range validation failed",
                        .transport_error = TransportError::FRAME_ENCODING_ERROR,
                        .frame_type = ack.type.type_detail(),
                    };
                }
                error::Error err;
                auto remove_acked = [&](status::PacketNumberSpace space, auto&& is_ack, auto&& then) {
                    auto& p = sent_packets[space_to_index(space)];
                    for (auto it = p.begin(); it != p.end();) {
                        SentPacket& sent = it->second;
                        // is_ack = bool(packetnum::Value,std::uint64_t sent_bytes,time::Time time_sent,packet::PacketStatus)
                        if (is_ack(sent.packet_number, sent.sent_bytes, sent.time_sent, sent.status)) {
                            if (sent.skiped) {
                                err = QUICError{
                                    .msg = "skipped packet number is acknoledged",
                                    .transport_error = TransportError::PROTOCOL_VIOLATION,
                                    .frame_type = ack.type.type_detail(),
                                };
                                return false;
                            }
                            acked.push_back(std::move(sent));
                            it = p.erase(it);
                            continue;
                        }
                        it++;
                    }
                    return then([&](auto&& apply_ack) {
                        for (auto& r : acked) {
                            apply_ack(r.sent_bytes, r.time_sent, r.status);
                            mark_as_ack(r.waiters);
                        }
                    });
                };
                status::ECNCounts ecn;
                bool has_ecn = false;
                if (ack.type.type_detail() == FrameType::ACK_ECN) {
                    ecn.ect0 = ack.ecn_counts.ect0;
                    ecn.ect1 = ack.ecn_counts.ect1;
                    ecn.ecn_ce = ack.ecn_counts.ecn_ce;
                    has_ecn = true;
                }
                auto res = status.on_ack_received(
                    space, ack.ack_delay, has_ecn ? &ecn : nullptr,
                    ranges, remove_lost_callback(err, lost), is_flow_control_limited, remove_acked);
                if (!res) {
                    if (err) {
                        return err;
                    }
                    return QUICError{
                        .msg = "internal state error. library bug!!",
                    };
                }
                return error::none;
            }

            template <class Alg>
            error::Error maybe_loss_detection_timeout(status::Status<Alg>& status, RemovedPackets* lost) {
                if (!status.is_loss_timeout()) {
                    return error::none;
                }
                error::Error err;
                if (!status.on_loss_detection_timeout(remove_lost_callback(err, lost))) {
                    if (err) {
                        return err;
                    }
                    return QUICError{
                        .msg = "internal state error. library bug!!",
                    };
                }
                return error::none;
            }

            template <class Alg>
            void on_packet_number_space_discarded(status::Status<Alg>& status, status::PacketNumberSpace space) {
                status.on_packet_number_space_discard(space, [&](auto& apply_remove) {
                    for (auto& r : sent_packets[space_to_index(space)]) {
                        apply_remove(r.second.sent_bytes, r.second.status);
                        mark_as_lost(r.second.waiters);
                    }
                    sent_packets[space_to_index(space)].clear();
                });
            }

            template <class Alg>
            void on_retry_received(status::Status<Alg>& status) {
                status.on_retry_received([&](auto&& apply_remove) {
                    auto& initial = sent_packets[space_to_index(status::PacketNumberSpace::initial)];
                    for (auto& r : initial) {
                        if (r.second.skiped) {
                            continue;
                        }
                        apply_remove(r.second.time_sent);
                        mark_as_lost(r.second.waiters);
                    }
                    initial.clear();
                    auto& app = sent_packets[space_to_index(status::PacketNumberSpace::application)];
                    for (auto& r : app) {
                        mark_as_lost(r.second.waiters);
                    }
                    app.clear();
                });
            }
        };

    }  // namespace fnet::quic::ack
}  // namespace utils

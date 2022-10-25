/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// rtt_sample
#pragma once
#include <cstddef>
#include <ratio>
#include <chrono>
#include <map>
#include "../../dll/allocator.h"
#include "../boxbytelen.h"
#include "../frame.h"
#include "../transport_param.h"
#include "../../closure.h"

namespace utils {
    namespace dnet {
        namespace quic::ack {

            enum class PacketNumberSpace {
                initial,
                handshake,
                application,
            };

            constexpr auto abs(auto a) {
                return a < 0 ? -a : a;
            }

            constexpr auto maximum(auto a, auto b) {
                return a > b ? a : b;
            }

            constexpr auto minimum(auto a, auto b) {
                return a < b ? a : b;
            }

            constexpr auto recommended_kTimeThreshold = std::ratio<9, 8>{};
            constexpr auto recommended_kGranularity = std::chrono::milliseconds(1);
            constexpr auto recommended_kPersistentCongestionThreshold = 3;

            using Time = std::chrono::system_clock::time_point;
            using Duration = std::chrono::system_clock::duration;

            struct LossDetectionTimer {
                Time timeout;
                void reset() {
                    timeout = {};
                }

                void update(Time time) {
                    timeout = time;
                }

                void cancel() {}
            };

            struct PacketNumber {
                size_t packet_number = 0;

                constexpr auto operator<=>(const PacketNumber&) const = default;
                constexpr PacketNumber operator+(const PacketNumber& pn) {
                    return {packet_number + pn.packet_number};
                }
            };

            struct PacketInfo {
                PacketNumber packet_number;
                Time time_sent;
                bool ack_eliciting = false;
                bool in_flight = false;
                BoxByteLen sent_payload;
                bool skipedPacket = false;
            };

            struct PacketNumberSpaceInfo {
                PacketNumber largest_acked_packet = {~0};
                Time time_of_last_ack_eliciting_packet = {};
                Time loss_time = {};
                using Packets = std::map<PacketNumber, PacketInfo, std::less<>, glheap_objpool_allocator<std::pair<const PacketNumber, PacketInfo>>>;
                Packets packets;

                size_t ack_eliciting_in_flight = 0;
                size_t ecn_ce_counters = 0;

                size_t largest_pn = 0;
            };

            // reference
            // RFC 9002 QUIC Loss Detection and Congestion Control
            // Appindex A Loss Recovery Pseudocode
            // Appindex B Congestion Control Pseudocode
            struct AckHandler {
                Duration kInitialRTT{};
                Duration kGranularity{};
                size_t kTimeThreshold = 0;
                size_t kPacketThreshold = 0;
                size_t kInitialWindow = 0;
                size_t kLossReductionFactor = 0;
                size_t kMinimumWindow = 0;

                LossDetectionTimer loss_detection_timer;
                size_t pto_count = 0;
                Duration latest_rtt{};
                Duration smoothed_rtt{};
                Duration rttvar{};
                Duration min_rtt{};
                Time first_rtt_sample{};
                DefinedTransportParams* tpparam = nullptr;
                PacketNumberSpaceInfo pn_spaces[3]{};

                size_t bytes_in_flight = 0;
                size_t congestion_window = 0;
                Time congestion_recovery_start_time{};
                size_t ssthresh = ~0;

                size_t ack_eliciting_in_flight = 0;
                bool has_handshake_keys = false;
                bool handshake_confirmed = false;
                bool server_is_at_anti_amplification_limit = false;
                bool is_server = false;

                closure::Closure<void> send_one_ack_eliciting_handshake_packet;
                closure::Closure<void> send_one_ack_eliciting_padded_initial_packet;
                closure::Closure<void, PacketNumberSpace> send_one_or_two_ack_eliciting_packet;
                closure::Closure<void> may_be_send_one_packet;

                static auto now() {
                    return std::chrono::system_clock::now();
                }

                void init() {
                    // init loss recovery
                    loss_detection_timer.reset();
                    pto_count = 0;
                    smoothed_rtt = kInitialRTT;
                    rttvar = kInitialRTT / 2;
                    min_rtt = {};
                    first_rtt_sample = {};
                    ack_eliciting_in_flight = 0;
                    // init congestion control
                    congestion_window = kInitialWindow;
                    bytes_in_flight = 0;
                    congestion_recovery_start_time = {};
                    ssthresh = ~0;

                    for (auto& pn_space : pn_spaces) {
                        pn_space.largest_acked_packet = PacketNumber{~size_t(0)};
                        pn_space.time_of_last_ack_eliciting_packet = {};
                        pn_space.loss_time = {};
                        pn_space.ack_eliciting_in_flight = 0;
                        pn_space.ecn_ce_counters = 0;
                    }
                }

                bool on_packet_sent(PacketNumber packet_number, PacketNumberSpace space, bool ack_eliciting, bool in_flight, BoxByteLen sent_payload) {
                    auto& pn_space = pn_spaces[int(space)];
                    auto& packet = pn_space.packets[packet_number];
                    packet.packet_number = packet_number;
                    packet.time_sent = now();
                    packet.ack_eliciting = ack_eliciting;
                    packet.in_flight = in_flight;
                    packet.sent_payload = std::move(sent_payload);
                    if (in_flight) {
                        if (ack_eliciting) {
                            pn_space.time_of_last_ack_eliciting_packet = packet.time_sent;
                            ack_eliciting_in_flight++;
                            pn_space.ack_eliciting_in_flight++;
                        }
                        on_packet_sent_cc(sent_payload.len());
                        return set_loss_detection_timer();
                    }
                    return true;
                }

                bool on_datagram_received() {
                    if (server_is_at_anti_amplification_limit) {
                        if (!set_loss_detection_timer()) {
                            return false;
                        }
                        if (loss_detection_timer.timeout < now()) {
                            return on_loss_detection_timeout();
                        }
                    }
                    return true;
                }

                std::pair<Time, PacketNumberSpace> get_loss_time_and_space() {
                    auto time = pn_spaces[int(PacketNumberSpace::initial)].loss_time;
                    auto space = PacketNumberSpace::initial;
                    auto& pn_handshake = pn_spaces[int(PacketNumberSpace::handshake)];
                    if (time == Time{} || pn_handshake.loss_time < time) {
                        time = pn_handshake.loss_time;
                        space = PacketNumberSpace::handshake;
                    }
                    auto& pn_application = pn_spaces[int(PacketNumberSpace::application)];
                    if (time == Time{} || pn_application.loss_time < time) {
                        time = pn_application.loss_time;
                        space = PacketNumberSpace::application;
                    }
                    return {time, space};
                }

                std::pair<Time, PacketNumberSpace> get_pto_time_and_space(bool& err) {
                    auto duration = Duration((smoothed_rtt + maximum(4 * rttvar, kGranularity)) *
                                             (1 << pto_count));
                    if (ack_eliciting_in_flight == 0) {
                        if (!peer_completed_address_validation()) {
                            err = true;
                            return {};
                        }
                        if (has_handshake_keys) {
                            return {now() + duration, PacketNumberSpace::handshake};
                        }
                        else {
                            return {now() + duration, PacketNumberSpace::initial};
                        }
                    }
                    auto pto_timeout = Time() + Duration(~0);
                    auto pto_space = PacketNumberSpace::initial;
                    for (auto i = 0; i < 3; i++) {
                        auto& pn_space = pn_spaces[i];
                        if (pn_space.ack_eliciting_in_flight == 0) {
                            continue;
                        }
                        if (PacketNumberSpace(i) == PacketNumberSpace::application) {
                            if (!handshake_confirmed) {
                                return {pto_timeout, pto_space};
                            }
                            duration += Duration(tpparam->max_ack_delay * (1 << pto_count));
                        }
                        auto t = pn_space.time_of_last_ack_eliciting_packet + duration;
                        if (t < pto_timeout) {
                            pto_timeout = t;
                            pto_space = PacketNumberSpace(i);
                        }
                    }
                    return {pto_timeout, pto_space};
                }

                bool peer_completed_address_validation() {
                    if (is_server) {
                        return true;
                    }
                    return handshake_confirmed;
                }

                bool set_loss_detection_timer() {
                    auto [earliest_loss_time, _] = get_loss_time_and_space();
                    if (earliest_loss_time != Time{}) {
                        loss_detection_timer.update(earliest_loss_time);
                        return true;
                    }
                    if (server_is_at_anti_amplification_limit) {
                        loss_detection_timer.cancel();
                        return true;
                    }
                    if (ack_eliciting_in_flight == 0 && peer_completed_address_validation()) {
                        loss_detection_timer.cancel();
                        return true;
                    }
                    bool err = false;
                    auto [timeout, _] = get_pto_time_and_space(err);
                    if (err) {
                        return false;
                    }
                    loss_detection_timer.update(timeout);
                    return true;
                }

                using PacketList = std::vector<PacketInfo, glheap_allocator<PacketInfo>>;

                auto detect_and_remove_lost_packet(PacketNumberSpace space) {
                    auto& pn_space = pn_spaces[int(space)];
                    pn_space.loss_time = Time();
                    PacketList lost_packets;
                    auto loss_delay = kTimeThreshold * maximum(latest_rtt, smoothed_rtt);
                    loss_delay = maximum(loss_delay, kGranularity);
                    auto lost_send_time = now() - Duration(loss_delay);
                    auto& packets = pn_space.packets;
                    bool erased = false;
                    auto incr = [&](auto& it) {
                        if (!erased) {
                            it++;
                        }
                        erased = false;
                    };
                    for (auto it = packets.begin(); it != packets.end(); incr(it)) {
                        auto& unacked = it->second;
                        if (unacked.packet_number > pn_space.largest_acked_packet) {
                            continue;
                        }
                        if (unacked.time_sent <= lost_send_time ||
                            pn_space.largest_acked_packet >=
                                unacked.packet_number + PacketNumber{kPacketThreshold}) {
                            lost_packets.push_back(std::move(unacked));
                            it = packets.erase(it);
                            erased = true;
                        }
                        else {
                            if (pn_space.loss_time == Time{}) {
                                pn_space.loss_time = unacked.time_sent + Duration(loss_delay);
                            }
                            else {
                                pn_space.loss_time = minimum(pn_space.loss_time, unacked.time_sent + Duration(loss_delay));
                            }
                        }
                    }
                    return lost_packets;
                }

                bool on_loss_detection_timeout() {
                    auto [earliest_loss_time, space] = get_loss_time_and_space();
                    if (earliest_loss_time != Time{}) {
                        auto lost_packets = detect_and_remove_lost_packet(space);
                        if (lost_packets.empty()) {
                            return false;
                        }
                        on_packets_lost(lost_packets);
                        return set_loss_detection_timer();
                    }
                    if (ack_eliciting_in_flight == 0) {
                        if (peer_completed_address_validation()) {
                            return false;
                        }
                        if (has_handshake_keys) {
                            if (send_one_ack_eliciting_handshake_packet) {
                                send_one_ack_eliciting_handshake_packet();
                            }
                        }
                        else {
                            if (send_one_ack_eliciting_padded_initial_packet) {
                                send_one_ack_eliciting_padded_initial_packet();
                            }
                        }
                    }
                    else {
                        bool err = false;
                        auto [_, space] = get_pto_time_and_space(err);
                        if (err) {
                            return false;
                        }
                        if (send_one_or_two_ack_eliciting_packet) {
                            send_one_or_two_ack_eliciting_packet(space);
                        }
                    }
                    pto_count++;
                    return set_loss_detection_timer();
                }

                using ACKRanges = std::vector<ACKRange, glheap_allocator<ACKRange>>;

                // reference implementation
                // https://github.com/lucas-clemente/quic-go/tree/0b26365daec94f36e36e082a9492145bf451c220/internal/ackhandler
                PacketList detect_and_remove_acked_packet(bool& err, const ACKFrame& ack, PacketNumberSpace space) {
                    auto& pn_space = pn_spaces[int(space)];
                    auto& packets = pn_space.packets;
                    ACKRanges ranges;
                    if (!get_ackranges(ranges, ack)) {
                        err = true;
                        return {};
                    }
                    auto lowest = PacketNumber(ranges[ranges.size() - 1].smallest);
                    auto largest = PacketNumber(ranges[0].largest);
                    size_t ackRangesIndex = 0;
                    PacketList acked;
                    for (auto& pc : packets) {
                        auto& p = pc.second;
                        if (p.packet_number < lowest ||
                            largest < p.packet_number) {
                            continue;
                        }
                        if (ranges.size() > 1) {
                            ACKRange ackRange = ranges[ranges.size() - 1 - ackRangesIndex];
                            while (p.packet_number > PacketNumber(ackRange.largest) &&
                                   ackRangesIndex < ranges.size()) {
                                ackRangesIndex++;
                                ackRange = ranges[ranges.size() - 1 - ackRangesIndex];
                            }
                            if (p.packet_number < PacketNumber(ackRange.smallest)) {
                                continue;  // nothing to do
                            }
                            if (p.packet_number > PacketNumber(ackRange.largest)) {
                                err = true;  // BUG
                                return {};
                            }
                        }
                        if (p.skipedPacket) {
                            err = true;
                            return {};
                        }
                        acked.push_back(std::move(p));
                    }
                    for (auto& p : acked) {
                        packets.erase(p.packet_number);
                    }
                    return acked;
                }

                static bool include_ack_eliciting(auto& vec) {
                    for (PacketInfo& p : vec) {
                        if (p.ack_eliciting) {
                            return true;
                        }
                    }
                    return false;
                }

                bool on_ack_received(const ACKFrame& ack, PacketNumberSpace space) {
                    auto& pn_space = pn_spaces[int(space)];
                    if (pn_space.largest_acked_packet == PacketNumber(~0)) {
                        pn_space.largest_acked_packet = PacketNumber(ack.largest_ack.qvarint());
                    }
                    else {
                        pn_space.largest_acked_packet = maximum(pn_space.largest_acked_packet,
                                                                PacketNumber(ack.largest_ack.qvarint()));
                    }
                    bool err = false;
                    auto acked = detect_and_remove_acked_packet(err, ack, space);
                    if (err) {
                        return false;
                    }
                    if (acked.empty()) {
                        return true;  // nothing to do
                    }
                    std::ranges::sort(acked, [](auto& a, auto& b) {
                        return a.packet_number > b.packet_number;
                    });
                    if (acked[0].packet_number == PacketNumber(ack.largest_ack.qvarint()) &&
                        include_ack_eliciting(acked)) {
                        latest_rtt = now() - acked[0].time_sent;
                        update_rtt(ack.ack_delay.qvarint());
                    }
                    if (ack.type.type_detail() == FrameType::ACK_ECN) {
                        if (!process_ecn(ack, space)) {
                            return false;
                        }
                    }
                    auto lost_packets = detect_and_remove_lost_packet(space);
                    if (!lost_packets.empty()) {
                        on_packets_lost(lost_packets);
                    }
                    on_packet_acked(acked);
                    if (peer_completed_address_validation()) {
                        pto_count = 0;
                    }
                    return set_loss_detection_timer();
                }

                void update_rtt(size_t ack_delay) {
                    if (first_rtt_sample == Time{}) {
                        min_rtt = latest_rtt;
                        smoothed_rtt = latest_rtt;
                        rttvar = latest_rtt / 2;
                        first_rtt_sample = now();
                        return;
                    }
                    min_rtt = minimum(min_rtt, latest_rtt);
                    if (handshake_confirmed) {
                        ack_delay = minimum(ack_delay, tpparam->max_ack_delay);
                    }
                    auto adjusted_rtt = latest_rtt;
                    if (latest_rtt >= min_rtt + Duration(ack_delay)) {
                        adjusted_rtt = latest_rtt - Duration(ack_delay);
                    }
                    rttvar = (3 * rttvar + abs(smoothed_rtt - adjusted_rtt)) / 4;
                    smoothed_rtt = (7 * smoothed_rtt + adjusted_rtt) / 8;
                }

                void on_packet_sent_cc(size_t sent_bytes) {
                    bytes_in_flight += sent_bytes;
                }

                bool in_persistent_congestion(PacketList& losts) {
                    // TODO(on-keyday): write persisten congestion detection
                    return false;
                }

                bool is_app_or_flow_control_limit() {
                    // TODO(on-keyday): limit?
                    return false;
                }

                void on_packets_lost(PacketList& packets) {
                    Time sent_time_of_last_loss{};
                    for (PacketInfo& p : packets) {
                        if (p.in_flight) {
                            bytes_in_flight -= p.sent_payload.unbox().len;
                            sent_time_of_last_loss = maximum(sent_time_of_last_loss, p.time_sent);
                        }
                    }
                    if (sent_time_of_last_loss != Time{}) {
                        on_congestion_event(sent_time_of_last_loss);
                    }
                    if (first_rtt_sample == Time{}) {
                        return;
                    }
                    PacketList losts;
                    for (PacketInfo& p : packets) {
                        if (p.time_sent > first_rtt_sample) {
                            losts.push_back(std::move(p));
                        }
                    }
                    if (in_persistent_congestion(losts)) {
                        congestion_window = kMinimumWindow;
                        congestion_recovery_start_time = {};
                    }
                }

                bool in_congestuion_recovery(auto&& sent_time) {
                    return sent_time <= congestion_recovery_start_time;
                }

                void on_packet_acked(PacketList& packets) {
                    for (auto& p : packets) {
                        if (!p.in_flight) {
                            continue;
                        }
                        auto len = p.sent_payload.unbox().len;
                        bytes_in_flight -= len;
                        if (is_app_or_flow_control_limit()) {
                            continue;
                        }
                        if (in_congestuion_recovery(p.time_sent)) {
                            continue;
                        }
                        if (congestion_window < ssthresh) {
                            // slow start
                            congestion_window += len;
                        }
                        else {
                            congestion_window += tpparam->max_udp_payload_size * len / congestion_window;
                        }
                    }
                }

                bool process_ecn(const ACKFrame& ack, PacketNumberSpace space) {
                    return ACKFrame::parse_ecn_counts(ack.ecn_counts, [&](auto ect0, auto ect1, auto ce) {
                        auto& pn_space = pn_spaces[int(space)];
                        pn_space.ecn_ce_counters = ce;
                        auto sent = pn_space.packets[PacketNumber(ack.largest_ack.qvarint())].time_sent;
                        on_congestion_event(sent);
                    });
                }

                void on_congestion_event(Time sent_time) {
                    if (in_congestuion_recovery(sent_time)) {
                        return;
                    }
                    congestion_recovery_start_time = now();
                    ssthresh = congestion_window * kLossReductionFactor;
                    congestion_window = maximum(ssthresh, kMinimumWindow);
                    if (may_be_send_one_packet) {
                        may_be_send_one_packet();
                    }
                }

                static size_t ilog2(size_t a) {
                    if (a == 1) {
                        return 0;
                    }
                    for (auto i = 0; i < 64; i++) {
                        const size_t bit = size_t(1) << (63 - i);
                        if (a & bit) {
                            return 63 - i;
                        }
                    }
                    return ~0;
                }

                size_t encode_packet_number_bytes(size_t pn, PacketNumberSpace space) {
                    auto largest = pn_spaces[int(space)].largest_acked_packet;
                    size_t num_acked = 0;
                    if (largest == PacketNumber{~size_t(0)}) {
                        num_acked = pn + 1;
                    }
                    else {
                        num_acked = pn - largest.packet_number;
                    }
                    auto min_bits = ilog2(num_acked);
                    auto num_bytes = min_bits / 8 + (min_bits % 8 ? 1 : 0);
                    return num_bytes;
                }

                size_t decode_packet_number(PacketNumberSpace space, size_t truncated, size_t pn_nbits) {
                    auto expected_pn = pn_spaces[int(space)].largest_pn + 1;
                    auto pn_win = size_t(1) << pn_nbits;
                    auto pn_hwin = pn_win >> 1;
                    auto pn_mask = pn_win - 1;
                    auto candidate_pn = (expected_pn & ~pn_mask) | truncated;
                    if (candidate_pn <= expected_pn - pn_hwin &&
                        candidate_pn < (1 << 62) - pn_win) {
                        return candidate_pn + pn_win;
                    }
                    if (candidate_pn > expected_pn + pn_hwin &&
                        candidate_pn >= pn_win) {
                        return candidate_pn - pn_win;
                    }
                    return candidate_pn;
                }
            };

        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

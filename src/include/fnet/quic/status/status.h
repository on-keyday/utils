/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "config.h"
#include "rtt.h"
#include "idle_timer.h"
#include "handshake.h"
#include "pto.h"
#include "congestion.h"
#include "pacer.h"
#include "pn_manage.h"
#include "loss_timer.h"
#include "ack_range.h"

namespace utils {
    namespace fnet::quic::status {

        template <class Alg>
        struct Status {
           private:
            InternalConfig config;
            HandshakeStatus hs;
            PayloadSize payload_size;
            std::uint64_t peer_ack_delay_exponent = default_ack_delay_exponent;
            time::Time creation_time;
            RTT rtt;
            Congestion<Alg> cong;

            // timer systems
            IdleTimer idle;
            LossTimer loss;
            PTOStatus pto;
            TokenBudgetPacer<Alg> pacer;
            time::Timer close_timer;
            time::Timer ping_timer;

            PacketNumberIssuer pn_issuers[3];
            PacketNumberAcceptor pn_acceptors[3];
            SentAckTracker sent_ack_tracker;

            constexpr void on_pto_timeout() {
                if (no_ack_eliciting_in_flight(pn_issuers) &&
                    !hs.peer_completed_address_validation()) {
                    if (hs.handshake_packet_is_sent()) {
                        pto.on_pto_no_flight(PacketNumberSpace::handshake);
                    }
                    else {
                        pto.on_pto_no_flight(PacketNumberSpace::initial);
                    }
                }
                else {
                    pto.on_pto_timeout(loss.current_space());
                }
            }

            constexpr time::Time loss_delay() const {
                auto time_threshold = config.time_threshold;
                if (config.time_threshold.den == 0) {
                    return time::invalid;
                }
                auto k = (config.time_threshold.num * min_(rtt.smoothed_rtt(), rtt.latest_rtt()));
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
                return max_(candidate, config.clock.to_clock_granularity(1));
            }

            constexpr bool detect_and_remove_lost_packet(auto&& remove_lost_packets, PacketNumberSpace loss_space, bool must_lost) {
                // --> code in detectAndRemoveLostPackets
                auto& issuer = pn_issuers[space_to_index(loss_space)];
                const auto delay = loss_delay();
                if (!delay.valid()) {
                    return false;
                }
                const auto time_before_is_lost = config.clock.now() - delay;
                const auto largest_ack = issuer.largest_acked_packet_number();
                loss.reset_loss_time(loss_space);
                time::time_t sent_time_of_last_loss = 0;
                bool call_lost_once = false;
                bool call_cong_cb_once = false;
                auto is_lost = [&](packetnum::Value pn, std::uint64_t sent_bytes, time::Time time_sent, packet::PacketStatus status) {
                    if (call_cong_cb_once) {  // congestion_callback already called
                        return LostReason::invalid;
                    }
                    if (pn > largest_ack) {  // maybe not acked yet
                        return LostReason::not_lost;
                    }
                    const bool time_threshold = time_sent <= time_before_is_lost;
                    const bool packet_order_threshold = pn + config.packet_order_threshold <= largest_ack;
                    if (time_threshold || packet_order_threshold) {  // maybe lost
                        issuer.on_packet_lost(status);
                        cong.on_packet_lost(sent_time_of_last_loss, sent_bytes, time_sent, status);
                        call_lost_once = true;
                        return time_threshold
                                   ? LostReason::time_threshold
                                   : LostReason::packet_order_threshold;
                    }
                    loss.update_loss_time(loss_space, time_sent + delay);
                    return LostReason::not_lost;
                };
                auto congestion_callback = [&](auto&& persistent_callback) {
                    if (call_cong_cb_once) {
                        return;
                    }
                    if (!call_lost_once) {
                        return;
                    }
                    cong.on_packets_lost(sent_time_of_last_loss, config, payload_size, rtt, pto, persistent_callback);
                    call_cong_cb_once = true;
                };
                // <-- code in detectAndRemoveLostPackets
                if (!remove_lost_packets(loss_space, is_lost, congestion_callback)) {
                    return false;
                }
                congestion_callback([&](auto&&...) {});  // call congestion callback once
                return !must_lost || call_lost_once;
            }

            constexpr void set_loss_detection_timer() {
                loss.set_loss_detection_timer(config, pto, rtt, hs, pn_issuers, ping_timer);
            }

            constexpr void process_ecn(PacketNumberIssuer& issuer, ECNCounts& ecn, time::Time time_sent) {
                // TODO(on-keyday): add ecn info
                cong.on_congestion_event(config, payload_size, time_sent);
            }

           public:
            // reset

            constexpr void reset(const InternalConfig& conf, Alg&& alg, bool is_server, std::uint64_t max_udp_payload) {
                config = conf;
                peer_ack_delay_exponent = default_ack_delay_exponent;
                // handshake
                hs.reset(is_server);
                // RTT
                rtt.reset(config);
                // congestions
                payload_size.reset(max_udp_payload);
                cong.reset(config, payload_size, std::move(alg));
                // timers
                loss.reset();
                idle.reset();
                pto.reset();
                pacer.reset();
                close_timer.cancel();
                ping_timer.cancel();
                // packet numbers
                for (auto& r : pn_issuers) {
                    r.reset();
                }
                for (auto& r : pn_acceptors) {
                    r.reset();
                }
                sent_ack_tracker.reset();
                creation_time = config.clock.now();
            }

            // each event handlers

            // on_transport_parameter_recived should be called when transport parameter is received
            constexpr void on_transport_parameter_received(std::uint64_t idle_timeout, std::uint64_t max_ack_delay, std::uint64_t ack_delay_exponent) {
                rtt.apply_max_ack_delay(config.clock.to_clock_granularity(max_ack_delay));
                peer_ack_delay_exponent = ack_delay_exponent;
                idle.apply_idle_timeout(config, idle_timeout);
            }

            constexpr void on_0RTT_transport_parameter(std::uint64_t idle_timeout) {
                idle.apply_idle_timeout(config, idle_timeout);
            }

            // on_payload_size_update should be called when PMTU is updated
            // if new_payload_size is smaller than current payload size, value will be ignored
            constexpr void on_payload_size_update(std::uint64_t new_paylod_size) {
                const auto old_size = payload_size.current_max_payload_size();
                if (payload_size.update_payload_size(new_paylod_size)) {
                    cong.on_update_max_udp_payload_size(config, old_size, new_paylod_size);
                }
            }

            // on_handshake_confirmed should be called when handshake confirmed:
            // server: TLS handshake done (automatically called when on_handshake_complete is called)
            // client: HANDSHAKE_DONE frame received
            constexpr void on_handshake_confirmed() {
                hs.on_handshake_confirmed();
            }

            // on_handshake_confirmed should be called when TLS handshake complete
            constexpr void on_handshake_complete() {
                hs.on_handshake_complete();
                if (hs.is_server()) {
                    on_handshake_confirmed();
                }
            }

            // on_packet_sent should be called when packet is sent
            // this function returns [prev_highest_sent+1,current_sent]
            // if failure, returns [-1,-1]
            constexpr expected<std::pair<std::int64_t, std::int64_t>> on_packet_sent(
                PacketNumberSpace space, packetnum::Value pn, std::uint64_t sent_bytes,
                time::Time time_sent, packet::PacketStatus status) {
                auto& pn_issuer = pn_issuers[space_to_index(space)];
                auto res = pn_issuer.on_packet_sent(config, pn, status);
                if (!res) {
                    return res;
                }
                hs.on_packet_sent(space, sent_bytes);
                idle.on_packet_sent(time_sent, status.is_ack_eliciting());
                pto.on_packet_sent(status.is_ack_eliciting());
                if (status.is_byte_counted()) {
                    cong.on_packet_sent(sent_bytes, time_sent);
                    pacer.on_packet_sent(config, payload_size, cong, rtt, time_sent, sent_bytes);
                    set_loss_detection_timer();
                }
                pacer.maybe_update_timer(config, cong, hs, payload_size, rtt);
                return res;
            }

            // on_datagram_received should be called when datagram received for this connection
            constexpr void on_datagram_received(std::uint64_t recv_bytes) {
                const auto was_limit = hs.is_at_anti_amplification_limit();
                hs.on_datagram_received(recv_bytes);
                if (was_limit && hs.is_at_anti_amplification_limit()) {
                    set_loss_detection_timer();
                }
            }

            // on_packet_decrypted should be called when encrypted packet is decrypted and parsed
            constexpr void on_packet_decrypted(PacketNumberSpace space) {
                hs.on_packet_decrypted(space);
                const auto now = config.clock.now();
                idle.on_packet_decrypted(now);
            }

            // on_packet_processed should be called when packet payload is processed completely
            constexpr void on_packet_processed(PacketNumberSpace space, packetnum::Value pn) {
                pn_acceptors[space_to_index(space)].on_packet_processed(pn);
            }

            // on_loss_detection_timeout should be called when is_loss_timeout() returns true
            // remove_lost_packet should be bool(PacketNumberSpace,auto&& is_lost,auto&& congestion_callback)
            // is_lost is LostReason(packetnum::Value,std::uint64_t sent_bytes,time::Time time_sent,packet::PacketStatus,bool is_mtu_probe)
            // congestion_callback is void(auto&& persistent_callback)
            // persistent_callback should be void(auto&& is_persistent_congestion)
            // is_persistent_congestion is bool(time::Time time_sent_earliest,time::Time time_sent_latest)
            constexpr bool on_loss_detection_timeout(auto&& remove_lost_packets) {
                switch (loss.current_state()) {
                    case LossTimerState::wait_for_loss: {
                        if (!detect_and_remove_lost_packet(remove_lost_packets, loss.current_space(), true)) {
                            return false;
                        }
                        set_loss_detection_timer();
                        return true;
                    }
                    case LossTimerState::wait_for_pto: {
                        on_pto_timeout();
                        set_loss_detection_timer();
                        return true;
                    }
                    default:
                        break;
                }
                return false;
            }

            // on_ack_received should be called after ACKFrame parsed
            // ack_ranges MUST be array of ACKRange, have member function size() and operator[] and be sorted by descending order
            // remove_acked_packets should be bool(PacketNumberSpace,auto&& is_ack,auto&& then)
            // is_ack is bool (packetnum::Value pn, std::uint64_t sent_bytes, time::Time time_sent, packet::PacketStatus status)
            // then is bool(auto&& congetion_callback)
            // congestion_callback should be void(auto&& apply_ack)
            // apply_ack is void(std::uint64_t sent_bytes, time::Time time_sent, packet::PacketStatus status)
            // remove_lost_packets is same as on_loss_detection_timeout
            // is_flow_control_limited should be bool()
            // call of is_ack() which returns true and of apply_congestion() MUST call same number of times
            // example:
            // on_ack_received(PacketNumberSpace::initial,0,nullptr,ack_ranges,
            //                remove_lost_packets,is_flow_control_limited,[&](auto&& is_ack,auto&& then){
            //      // remove ack packets...
            //      return then([&](auto&& apply_ack){ // this functions is congestion_callback
            //         // apply removed packets parameters...
            //      });
            // });
            constexpr bool on_ack_received(
                PacketNumberSpace space, std::uint64_t ack_delay_wire, ECNCounts* ecn,
                const auto& ack_ranges,
                auto&& remove_lost_packets,
                auto&& is_flow_control_limited, auto&& remove_acked_packets) {
                hs.on_ack_received(space);
                auto& issuer = pn_issuers[space_to_index(space)];
                auto range_size = ack_ranges.size();
                if (range_size == 0) {
                    return false;  // invalid ack ranges
                }
                packetnum::Value largest_ack_pn = std::uint64_t(ack_ranges[0].largest);
                packetnum::Value smallest_ack_pn = std::uint64_t(ack_ranges[range_size - 1].smallest);
                issuer.on_ack_received(largest_ack_pn);
                bool has_ack_eliciting = false;
                packetnum::Value largest_removed = packetnum::infinity;
                time::Time largest_removed_sent_time;
                // post condition:
                //  [ack_count==after_ack_count]
                //  [after_call_once == 1]
                std::uint64_t ack_count = 0;
                std::uint64_t after_ack_count = 0;
                int after_call_once = 0;
                auto is_ack = [&](packetnum::Value pn, std::uint64_t sent_bytes, time::Time time_sent, packet::PacketStatus status, packetnum::Value largest_sent_ack) {
                    if (pn < smallest_ack_pn ||
                        largest_ack_pn < pn) {
                        return false;
                    }
                    if (range_size > 1) {
                        ACKRange range;
                        for (size_t i = 0; i < range_size; i++) {
                            auto r = ack_ranges[range_size - 1 - i];
                            if (pn < r.largest) {
                                range.largest = r.largest;
                                range.smallest = r.smallest;
                                break;
                            }
                        }
                        if (pn < range.smallest) {
                            return false;
                        }
                    }
                    issuer.on_packet_ack(status);
                    sent_ack_tracker.on_packet_acked(space, largest_sent_ack);
                    has_ack_eliciting = has_ack_eliciting || status.is_ack_eliciting();
                    if (largest_removed == packetnum::infinity || largest_removed < pn) {
                        largest_removed = pn;
                        largest_removed_sent_time = time_sent;
                    }
                    ack_count++;
                    return true;
                };
                bool after_call_res = false;
                auto after_remove_acked_packets = [&](auto&& then) {
                    if (after_call_once >= 1) {
                        after_call_once = 2;
                        return false;
                    }
                    after_call_once = 1;
                    if (largest_removed == largest_ack_pn && has_ack_eliciting) {
                        if (!rtt.sample_rtt(config, largest_removed_sent_time,
                                            config.clock.to_clock_granularity(decode_ack_delay(ack_delay_wire, peer_ack_delay_exponent)))) {
                            return false;  // internal failure
                        }
                    }
                    if (ecn) {
                        process_ecn(issuer, *ecn, largest_removed_sent_time);
                    }
                    if (!detect_and_remove_lost_packet(remove_lost_packets, space, false)) {
                        return false;  // fatal error?
                    }
                    auto congestion_callback = [&](std::uint64_t sent_bytes, time::Time time_sent, packet::PacketStatus status) {
                        cong.on_packet_ack(config, payload_size, sent_bytes, time_sent, status, is_flow_control_limited);
                        after_ack_count++;
                    };
                    then(congestion_callback);
                    pto.on_ack_received(hs);
                    set_loss_detection_timer();
                    after_call_res = true;
                    return true;
                };
                if (!remove_acked_packets(space, is_ack, after_remove_acked_packets)) {
                    return false;
                }
                if (ack_count != after_ack_count ||
                    after_call_once != 1) {
                    return false;
                }
                return after_call_res;
            }

            // remove_packets should be void(auto&& apply_remove)
            // apply_remove is void(std::uint64_t sent_size,packet::PacketStatus)
            constexpr void on_packet_number_space_discard(PacketNumberSpace space, auto&& remove_packets) {
                if (space != PacketNumberSpace::initial && space != PacketNumberSpace::handshake) {
                    return;
                }
                auto apply_remove = [&](std::uint64_t sent_size, packet::PacketStatus status) {
                    cong.on_packet_number_space_discard(sent_size, status);
                };
                remove_packets(apply_remove);
                pn_issuers[space_to_index(space)].on_packet_number_space_discard();
                loss.on_packet_number_space_discard(space);
                pto.on_packet_number_space_discard();
                set_loss_detection_timer();
            }

            // lost_sent_packet should be void(auto&& apply_remove)
            // apply_remove is void(time::Time time_sent)
            constexpr void on_retry_received(auto&& lost_sent_packet) {
                hs.on_retry_received();
                time::Time first_sent_time = time::invalid;
                lost_sent_packet([&](time::Time time_sent) {
                    if (!first_sent_time.valid()) {
                        first_sent_time = time_sent;
                    }
                });
                pto.on_retry_received(config, rtt, first_sent_time);
                loss.on_retry_received();
                pn_issuers[space_to_index(PacketNumberSpace::initial)].on_retry_received();
                pn_issuers[space_to_index(PacketNumberSpace::application)].on_retry_received();
            }

            // packet number issuer/consumer

            constexpr std::pair<packetnum::Value, packetnum::Value> next_and_largest_acked_packet_number(PacketNumberSpace space) const {
                auto& pn_issuer = pn_issuers[space_to_index(space)];
                return {pn_issuer.next_packet_number(), pn_issuer.largest_acked_packet_number()};
            }

            constexpr void consume_packet_number(PacketNumberSpace space) {
                pn_issuers[space_to_index(space)].consume_packet_number();
            }

            constexpr packetnum::Value largest_received_packet_number(PacketNumberSpace space) {
                return pn_acceptors[space_to_index(space)].largest_received_packet_number();
            }

            constexpr packetnum::Value get_onertt_largest_acked_sent_ack() const noexcept {
                return sent_ack_tracker.get_onertt_largest_acked_sent_ack();
            }

            // timeouts

            constexpr bool is_handshake_timeout() const {
                if (!creation_time.valid() || config.handshake_timeout == 0) {
                    return false;  // bug?
                }
                return config.clock.now() - creation_time >= config.clock.to_clock_granularity(config.handshake_timeout);
            }

            constexpr bool is_loss_timeout() const {
                return loss.timeout(config);
            }

            constexpr bool is_idle_timeout() const {
                return idle.timeout(config, hs);
            }

            constexpr bool can_send(PacketNumberSpace space) const {
                return pacer.can_send(config) || pto.is_probe_required(space) ||
                       cong.should_send_any_packet();
            }

            constexpr bool is_pto_probe_required(PacketNumberSpace space) const {
                return pto.is_probe_required(space);
            }

            constexpr bool should_send_any_packet() const {
                return cong.should_send_any_packet();
            }

            constexpr void set_close_timer() {
                const auto close_until = pto.probe_timeout_duration(config, rtt);
                close_timer.set_deadline(close_until);
            }

            constexpr bool close_timeout() const {
                return close_timer.timeout(config.clock);
            }

            constexpr time::Time get_close_deadline() const {
                return close_timer.get_deadline();
            }

            constexpr bool should_send_ping() const {
                return loss.current_state() == LossTimerState::no_timer &&
                       ping_timer.timeout(config.clock);
            }

            // for program consistency
            constexpr void on_handshake_start() {
                hs.on_handshake_start();
            }

            constexpr bool handshake_started() const {
                return hs.handshake_started();
            }

            constexpr bool handshake_complete() const {
                return hs.handshake_complete();
            }

            constexpr bool handshake_confirmed() const {
                return hs.handshake_confirmed();
            }

            constexpr void on_transport_parameter_read() {
                hs.on_transport_parameter_read();
            }

            constexpr bool transport_parameter_read() const {
                return hs.transport_parameter_read();
            }

            constexpr time::Time path_validation_deadline() const {
                auto timeout_candidate = config.path_validation_timeout_factor * pto.probe_timeout_duration_with_max_ack_delay(config, rtt);
                RTT initial_rtt;
                initial_rtt.reset(config);
                auto exponent = pto.pto_exponent();
                auto initial_candidate = calc_probe_timeout_duration(initial_rtt.smoothed_rtt(), initial_rtt.rttvar(), config.clock, exponent);
                return now() + max_(initial_candidate, timeout_candidate);
            }

            // exports

            constexpr time::Time now() const {
                return config.clock.now();
            }

            constexpr bool is_server() const {
                return hs.is_server();
            }

            constexpr bool has_received_retry() const {
                return hs.has_received_retry();
            }

            constexpr bool has_sent_retry() const {
                return hs.has_sent_retry();
            }

            constexpr void set_retry_sent() {
                hs.on_retry_sent();
            }

            constexpr const HandshakeStatus& handshake_status() const {
                return hs;
            }

            constexpr const RTT& rtt_status() const {
                return rtt;
            }

            constexpr const PTOStatus& pto_status() const {
                return pto;
            }

            constexpr const InternalConfig& status_config() const {
                return config;
            }

            constexpr const time::Clock& clock() const {
                return config.clock;
            }

            constexpr const LossTimer& loss_timer() const {
                return loss;
            }

            constexpr time::Time get_earliest_deadline(time::Time ack_delay) const {
                auto losst = loss.get_deadline();
                auto pingt = ping_timer.get_deadline();
                auto closet = close_timer.get_deadline();
                auto pacert = pacer.get_deadline();
                auto idlet = idle.get_deadline(config, hs);
                auto earliest = ack_delay;
                auto maybe_set = [&](time::Time t) {
                    if (t.valid() && (!earliest.valid() || t < earliest)) {
                        earliest = t;
                    }
                };
                maybe_set(losst);
                maybe_set(pingt);
                maybe_set(closet);
                maybe_set(pacert);
                maybe_set(idlet);
                return earliest;
            }
        };

        constexpr auto sizeof_Status = sizeof(Status<NullAlgorithm>);
    }  // namespace fnet::quic::status
}  // namespace utils

/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "config.h"
#include "rtt.h"
#include "pto.h"
#include "../packet/summary.h"

namespace futils {
    namespace fnet::quic::status {

        static constexpr std::uint64_t min_window(const InternalConfig& config, std::uint64_t max_udp_payload_size) {
            return config.window_minimum_factor * max_udp_payload_size;
        }

        struct WindowModifier {
           private:
            template <class Alg>
            friend struct Congestion;
            const InternalConfig* conf = nullptr;
            const PayloadSize* size = nullptr;
            std::uint64_t* window = nullptr;
            bool incr = false;
            constexpr WindowModifier(const InternalConfig* c, const PayloadSize* p, std::uint64_t* val, bool incr)
                : conf(c), size(p), window(val), incr(incr) {}

           public:
            constexpr std::uint64_t get_window() const {
                return *window;
            }

            constexpr std::uint64_t min_window() const {
                return status::min_window(*conf, size->current_max_payload_size());
            }

            constexpr std::uint64_t max_payload() const {
                return size->current_max_payload_size();
            }

            constexpr bool update(std::uint64_t new_window) {
                if (incr) {
                    if (new_window < *window) {
                        return false;
                    }
                }
                else {
                    if (new_window > *window) {
                        return false;
                    }
                }
                *window = new_window;
                return true;
            }
        };

        struct NullAlgorithm {
            constexpr void on_packet_sent(std::uint64_t sent_bytes, time::Time time_sent) {}
            constexpr void on_packet_ack(WindowModifier& modif, std::uint64_t sent_bytes, time::Time time_sent) {}
            constexpr void on_congestion_event(WindowModifier& modif, time::Time time_sent) {}
        };

        template <class Alg>
        struct Congestion {
           private:
            std::uint64_t congestion_window = 0;
            std::uint64_t bytes_in_flight = 0;
            time::Time congestion_recovery_start_time = 0;
            bool should_send_any = false;  // any packet should send
            Alg algorithm{};

           public:
            constexpr void reset(const InternalConfig& config, const PayloadSize& payload_size, Alg alg = {}) {
                congestion_window = config.window_initial_factor * payload_size.current_max_payload_size();
                bytes_in_flight = 0;
                congestion_recovery_start_time = 0;
                should_send_any = false;
                algorithm = std::move(alg);
            }

            constexpr void on_packet_sent(std::uint64_t sent_bytes, time::Time time_sent) {
                bytes_in_flight += sent_bytes;
                should_send_any = false;
                algorithm.on_packet_sent(sent_bytes, time_sent);
            }

            constexpr bool in_congestion_recovery(time::Time time) const {
                return time <= congestion_recovery_start_time;
            }

            constexpr void on_packet_ack(const InternalConfig& config, const PayloadSize& payload_size, std::uint64_t sent_bytes, time::Time time_sent, packet::PacketStatus status, auto&& is_flow_control_limited) {
                if (!status.is_byte_counted()) {
                    return;
                }
                bytes_in_flight -= sent_bytes;
                if (is_flow_control_limited()) {
                    return;  // avoid increasing
                }
                if (in_congestion_recovery(time_sent)) {
                    return;
                }
                WindowModifier modif{
                    &config,
                    &payload_size,
                    &congestion_window,
                    true,
                };
                algorithm.on_packet_ack(modif, sent_bytes, time_sent);
            }

            // on_packet_lost is called for each packet lost
            constexpr void on_packet_lost(time::time_t& sent_time_of_last_loss, std::uint64_t sent_bytes, time::Time time_sent, packet::PacketStatus status) {
                if (!status.is_ack_eliciting()) {
                    return;
                }
                bytes_in_flight -= sent_bytes;
                if (status.is_mtu_probe()) {  // MTU Probe packet loss MUST NOT affect congestion control
                    return;
                }
                sent_time_of_last_loss = max_(sent_time_of_last_loss, time_sent.to_int());
            }

            constexpr void on_congestion_event(const InternalConfig& config, const PayloadSize& payload_size, time::time_t time_sent) {
                if (in_congestion_recovery(time_sent)) {
                    return;
                }
                congestion_recovery_start_time = time_sent;
                WindowModifier modif{
                    &config,
                    &payload_size,
                    &congestion_window,
                    false,
                };
                algorithm.on_congestion_event(modif, time_sent);
                should_send_any = true;
            }

            // on_packets_lost is called after all on_packet_lost is called
            constexpr void on_packets_lost(
                time::time_t time_sent,
                const InternalConfig& config, const PayloadSize& payload_size,
                const RTT& rtt, const PTOStatus& pto, auto&& persistent_congestion_cb) {
                if (time_sent > 0) {
                    on_congestion_event(config, payload_size, time_sent);
                }
                if (!rtt.has_first_ack_sample()) {
                    return;
                }
                const auto first_ack_sample_time = rtt.first_ack_sample();
                const time::Time persistent_congestion_duration =
                    config.clock.now() -
                    (pto.probe_timeout_duration_with_max_ack_delay(config, rtt) * config.persistent_congestion_threshold);
                if (!persistent_congestion_duration.valid()) {
                    return;  // invalid clock?
                }
                auto is_persistent_congestion = [&](time::Time sent_time_earliest, time::Time sent_time_latest) {
                    if (sent_time_latest <= first_ack_sample_time ||
                        sent_time_earliest <= first_ack_sample_time) {
                        return false;
                    }
                    if ((sent_time_latest - sent_time_earliest) >= persistent_congestion_duration) {
                        congestion_window = min_window(config, payload_size.current_max_payload_size());
                        congestion_recovery_start_time = 0;
                        return true;
                    }
                    return false;
                };
                persistent_congestion_cb(is_persistent_congestion);
            }

            constexpr std::uint64_t bandwidth(const RTT& rtt) const {
                if (rtt.smoothed_rtt() == 0) {
                    return ~std::uint64_t(0);
                }
                return congestion_window / rtt.smoothed_rtt();
            }

            constexpr void on_update_max_udp_payload_size(const InternalConfig& config, std::uint64_t current_udp_payload_size, std::uint64_t new_udp_payload_size) {
                if (min_window(config, current_udp_payload_size) == congestion_window) {
                    congestion_window = min_window(config, new_udp_payload_size);
                }
            }

            constexpr void on_packet_number_space_discard(std::uint64_t decrease, packet::PacketStatus status) {
                if (status.is_byte_counted()) {
                    bytes_in_flight -= decrease;
                }
            }

            constexpr bool should_send_any_packet() const {
                return should_send_any;
            }

            constexpr void on_connection_migration(const InternalConfig& config, const PayloadSize& psize) {
                congestion_window = config.window_initial_factor * psize.current_max_payload_size();
            }
        };

        static_assert(sizeof(Congestion<NullAlgorithm>));

    }  // namespace fnet::quic::status
}  // namespace futils

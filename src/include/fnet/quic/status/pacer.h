/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "congestion.h"

namespace futils {
    namespace fnet::quic::status {
        // reference implementation
        // https://github.com/quic-go/quic-go/blob/master/internal/congestion/pacer.go
        template <class Alg>
        struct TokenBudgetPacer {
           private:
            time::Time last_sent_time = time::invalid;
            time::Timer timer;
            std::uint64_t budget_at_last_sent = 0;

           public:
            constexpr void reset() {
                last_sent_time = time::invalid;
                timer.cancel();
                budget_at_last_sent = 0;
            }

            constexpr bool can_send(const InternalConfig& config) const {
                return timer.not_working() || timer.timeout(config.clock);
            }

            constexpr time::Time get_deadline() const {
                return timer.get_deadline();
            }

            constexpr void on_packet_sent(const InternalConfig& config,
                                          const PayloadSize& payload_size,
                                          const Congestion<Alg>& cong,
                                          const RTT& rtt,
                                          time::Time time_sent, std::uint64_t size) {
                auto now_budget = budget(config, payload_size, cong, rtt, time_sent);
                if (size > now_budget) {
                    budget_at_last_sent = 0;
                }
                else {
                    budget_at_last_sent = now_budget - size;
                }
                last_sent_time = time_sent;
            }

            // maybe_update_timer should be called after on_packet_sent
            constexpr void maybe_update_timer(const InternalConfig& config,
                                              const Congestion<Alg>& cong,
                                              const HandshakeStatus& hs,
                                              const PayloadSize& payload_size,
                                              const RTT& rtt) {
                if (cong.should_send_any_packet() && hs.handshake_complete()) {
                    set_next_send_time(config, cong, payload_size, rtt);
                }
            }

            constexpr void set_next_send_time(
                const InternalConfig& config, const Congestion<Alg>& cong,
                const PayloadSize& payload_size,
                const RTT& rtt) {
                const auto current_max_payload_size = payload_size.current_max_payload_size();
                if (budget_at_last_sent >= current_max_payload_size) {
                    timer.cancel();
                    return;
                }
                const auto deltaf = double(current_max_payload_size - budget_at_last_sent) * 1e9 / adjusted_bandwidth(config, cong, rtt);
                const std::uint64_t delta = (deltaf / config.clock.to_nanosec(config.clock.to_clock_granularity(1))) + 0.9;  // ceil
                timer.set_deadline(last_sent_time + delta);
            }

            constexpr std::uint64_t budget(const InternalConfig& config, const PayloadSize& payload_size,
                                           const Congestion<Alg>& cong, const RTT& rtt, time::Time now) const {
                if (!last_sent_time.valid()) {
                    return max_burst_size(config, payload_size, cong, rtt);
                }
                auto next = budget_at_last_sent + ((adjusted_bandwidth(config, cong, rtt) *
                                                    (now - last_sent_time)) /
                                                   config.clock.to_clock_granularity(1000));
                return min_(max_burst_size(config, payload_size, cong, rtt), next);
            }

            constexpr std::uint64_t max_burst_size(const InternalConfig& config, const PayloadSize& paylod_size, const Congestion<Alg>& cong, const RTT& rtt) const {
                const auto a = (config.clock.to_clock_granularity(2) *
                                adjusted_bandwidth(config, cong, rtt)) /
                               (config.clock.to_clock_granularity(1000));
                return max_(a, config.window_initial_factor * paylod_size.current_max_payload_size());
            }

            constexpr std::uint64_t adjusted_bandwidth(const InternalConfig& config, const Congestion<Alg>& cong, const RTT& rtt) const {
                if (config.pacer_N.den == 0) {
                    return -1;
                }
                return config.pacer_N.num * cong.bandwidth(rtt) / config.pacer_N.den;
            }
        };
    }  // namespace fnet::quic::status
}  // namespace futils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "congestion.h"

namespace utils {
    namespace dnet::quic::ack {

        struct NewRenoCC {
            size_t persistent_congestion_threshold = 3;
            time_t persistent_duration(time::Clock& clock, RTT& rtt) {
                return rtt.smoothed_rtt_ + max_(rtt.rttvar_ << 2, clock.to_clock_granurarity(1)) *
                                               persistent_congestion_threshold;
            }
            size_t sstresh = ~0;
            Ratio loss_reducion_factor{1, 2};

            static void reset(std::shared_ptr<void>& ctrl, Congestion* c) {
                auto r = static_cast<NewRenoCC*>(ctrl.get());
                r->sstresh = ~0;
            }

            static error::Error on_congestion(std::shared_ptr<void>& ctrl, Congestion* c) {
                auto r = static_cast<NewRenoCC*>(ctrl.get());
                r->sstresh = r->loss_reducion_factor.num * c->congestion_window /
                             r->loss_reducion_factor.den;
                c->congestion_window = max_(r->sstresh, c->min_window());
                return error::none;
            }

            static error::Error on_ack(std::shared_ptr<void>& ctrl, Congestion* c, SentPacket* p) {
                auto r = static_cast<NewRenoCC*>(ctrl.get());
                if (c->congestion_window < r->sstresh) {
                    c->congestion_window += p->sent_bytes;
                }
                else {
                    c->congestion_window += c->max_udp_payload_size * p->sent_bytes / c->congestion_window;
                }
                return error::none;
            }

            static void set_handler(CongestionEventHandlers& h) {
                h.controller = std::allocate_shared<NewRenoCC>(glheap_allocator<NewRenoCC>{});
                h.reset_cb = reset;
                h.congestion_event_cb = on_congestion;
                h.ack_window_cb = on_ack;
            }
        };

        // reference implementation
        // https://github.com/quic-go/quic-go/blob/master/internal/congestion/pacer.go
        struct TokenBudgetPacer {
            Ratio N{5, 4};
            time::Time last_sent_time = time::invalid;
            time::Timer timer;
            std::uint64_t budget_at_last_sent = 0;

            bool can_send(time::Clock& clock) const {
                return !timer.get_deadline().valid() || timer.timeout(clock);
            }

            void on_packet_sent(Congestion& c, RTT& rtt, time::Clock& clock,
                                time::Time time_sent, std::uint64_t size) {
                auto now_budget = budget(c, rtt, clock, time_sent);
                if (size > now_budget) {
                    budget_at_last_sent = 0;
                }
                else {
                    budget_at_last_sent = now_budget - size;
                }
                last_sent_time = time_sent;
            }

            void set_next_send_time(Congestion& c, RTT& rtt, time::Clock& clock) {
                if (budget_at_last_sent >= c.max_udp_payload_size) {
                    timer.set_deadline(time::invalid);
                }
                const auto deltaf = double(c.max_udp_payload_size - budget_at_last_sent) * 1e9 / adjusted_bandwidth(c, rtt);
                const std::uint64_t delta = (deltaf / clock.to_nanosec(clock.to_clock_granurarity(1))) + 0.9;  // ceil
                timer.set_deadline(last_sent_time + delta);
            }

            std::uint64_t budget(Congestion& c, RTT& rtt, time::Clock& clock, time::Time now) {
                if (!last_sent_time.valid()) {
                    return max_burst_size(c, rtt, clock);
                }
                auto next = budget_at_last_sent + ((adjusted_bandwidth(c, rtt) *
                                                    (now.value - last_sent_time.value)) /
                                                   clock.to_clock_granurarity(1000));
                return min_(max_burst_size(c, rtt, clock), next);
            }

            std::uint64_t max_burst_size(Congestion& c, RTT& rtt, time::Clock& clock) {
                const auto a = (clock.to_clock_granurarity(2) * adjusted_bandwidth(c, rtt)) / (clock.to_clock_granurarity(1000));
                return max_(a, c.initial_factor * c.max_udp_payload_size);
            }

            std::uint64_t adjusted_bandwidth(Congestion& c, RTT& rtt) {
                if (N.den == 0) {
                    return -1;
                }
                return N.num * c.bandwidth(rtt) / N.den;
            }
        };

    }  // namespace dnet::quic::ack
}  // namespace utils

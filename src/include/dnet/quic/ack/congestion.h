/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// congestion - congestion control for QUIC
#pragma once
#include "rtt.h"
#include "../../closure.h"
#include "../transport_error.h"
#include "sent_packet.h"

namespace utils {
    namespace dnet {
        namespace quic::ack {
            struct Congestion;

            struct CongestionEventHandlers {
                std::shared_ptr<void> controller;
                error::Error (*congestion_event_cb)(std::shared_ptr<void>& arg, Congestion*);
                error::Error (*ack_window_cb)(std::shared_ptr<void>& arg, Congestion*, SentPacket*);
                error::Error (*persistent_congestion_cb)(std::shared_ptr<void>& arg, Congestion*, RemovedPackets*);

                std::weak_ptr<void> conn;
                bool (*flow_control_limit_cb)(std::shared_ptr<void>& conn);
                error::Error (*send_one_packet_cb)(std::shared_ptr<void>& conn);

                error::Error on_congestion_event(Congestion* c) {
                    if (congestion_event_cb) {
                        return congestion_event_cb(controller, c);
                    }
                    return error::none;
                }

                error::Error on_ack(Congestion* c, SentPacket* s) {
                    if (ack_window_cb) {
                        return ack_window_cb(controller, c, s);
                    }
                    return error::none;
                }

                error::Error on_persistent_congestion(Congestion* c, RemovedPackets* rem) {
                    if (persistent_congestion_cb) {
                        return persistent_congestion_cb(controller, c, rem);
                    }
                    return error::none;
                }

                bool is_on_flow_control_limit() {
                    if (flow_control_limit_cb) {
                        auto ctx = conn.lock();
                        if (!ctx) {
                            return false;
                        }
                        return flow_control_limit_cb(ctx);
                    }
                    return false;
                }

                error::Error maybe_send_one_packet() {
                    if (flow_control_limit_cb) {
                        auto ctx = conn.lock();
                        if (!ctx) {
                            return error::none;
                        }
                        return send_one_packet_cb(ctx);
                    }
                    return error::none;
                }
            };

            struct Congestion {
                // constant value
                std::uint64_t max_datagram_size = 0;
                std::uint64_t initial_window = 0;
                std::uint64_t minimum_window = 0;

                // controler value
                std::uint64_t congestion_window = 0;
                std::uint64_t bytes_in_flight = 0;
                time::Time congestion_recovery_start_time = 0;

                CongestionEventHandlers handlers;

                void reset() {
                    congestion_window = initial_window;
                    bytes_in_flight = 0;
                    congestion_recovery_start_time = 0;
                }

                void on_packet_sent(std::uint64_t sent_bytes) {
                    bytes_in_flight += sent_bytes;
                }

                bool in_congestion_recovery(time::Time time) {
                    return time <= congestion_recovery_start_time;
                }

                error::Error on_congestion_event(time::Clock& clock, time::Time time_sent) {
                    if (in_congestion_recovery(time_sent)) {
                        return error::none;
                    }
                    congestion_recovery_start_time = clock.now();
                    if (auto err = handlers.on_congestion_event(this)) {
                        return error::none;
                    }
                    return handlers.maybe_send_one_packet();
                }

                error::Error on_packets_ack(RemovedPackets& acked) {
                    for (auto& p : acked) {
                        auto err = on_packet_ack(p);
                        if (err) {
                            return err;
                        }
                    }
                    return error::none;
                }

                error::Error on_packet_ack(SentPacket& acked) {
                    if (!acked.in_flight) {
                        return error::none;
                    }
                    bytes_in_flight -= acked.sent_bytes;
                    mark_as_ack(acked.waiters);
                    if (handlers.is_on_flow_control_limit()) {
                        return error::none;
                    }
                    if (in_congestion_recovery(acked.time_sent)) {
                        return error::none;
                    }
                    return handlers.on_ack(this, &acked);
                }

                error::Error on_packets_lost(time::Clock& clock, RTT& rtt, RemovedPackets& packets) {
                    time_t sent_time_of_last_loss = 0;
                    for (auto& packet : packets) {
                        if (packet.in_flight) {
                            bytes_in_flight -= packet.sent_bytes;
                            sent_time_of_last_loss = max_(sent_time_of_last_loss, packet.time_sent.value);
                        }
                        mark_as_lost(packet.waiters);
                    }
                    if (sent_time_of_last_loss != 0) {
                        auto err = on_congestion_event(clock, sent_time_of_last_loss);
                        if (err) {
                            return err;
                        }
                    }
                    if (rtt.first_ack_sample == 0) {
                        return error::none;
                    }
                    RemovedPackets pc_lost;
                    for (auto& lost : packets) {
                        if (lost.time_sent > rtt.first_ack_sample) {
                            pc_lost.push_back(std::move(lost));
                        }
                    }
                    return handlers.on_persistent_congestion(this, &pc_lost);
                }
            };

            struct NewRenoCC {
                size_t persistent_congestion_threshold = 3;
                time_t persistent_duration(time::Clock& clock, RTT& rtt) {
                    return rtt.smoothed_rtt + max_(rtt.rttvar << 2, clock.granularity) *
                                                  persistent_congestion_threshold;
                }
                size_t sstresh = 0;
                size_t loss_reduction_factor = 0;

                static error::Error on_congestion(std::shared_ptr<void>& ctrl, Congestion* c) {
                    auto r = static_cast<NewRenoCC*>(ctrl.get());
                    c->congestion_window = max_(r->sstresh, c->minimum_window);
                    return error::none;
                }

                static error::Error on_ack(std::shared_ptr<void>& ctrl, Congestion* c, SentPacket* p) {
                    auto r = static_cast<NewRenoCC*>(ctrl.get());
                    if (c->congestion_window < r->sstresh) {
                        c->congestion_window += p->sent_bytes;
                    }
                    else {
                        c->congestion_window += c->max_datagram_size * p->sent_bytes / c->congestion_window;
                    }
                    return error::none;
                }

                static void set_handler(CongestionEventHandlers& h) {
                    h.controller = std::allocate_shared<NewRenoCC>(glheap_allocator<NewRenoCC>{});
                    h.congestion_event_cb = on_congestion;
                    h.ack_window_cb = on_ack;
                }
            };

            struct Pacer {
                time::utime_t N = 1;
                time::time_t rate(Congestion& c, RTT& rtt) {
                    if (rtt.smoothed_rtt <= 0) {
                        return -1;
                    }
                    return N * c.congestion_window / rtt.smoothed_rtt;
                }

                time::time_t interval(size_t packet_size, Congestion& c, RTT& rtt) {
                    if (N == 0 || c.congestion_window == 0) {
                        return -1;
                    }
                    return (rtt.smoothed_rtt * packet_size / c.congestion_window) / N;
                }
            };
        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

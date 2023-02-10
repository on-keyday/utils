/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// congestion - congestion control for QUIC
#pragma once
#include "rtt.h"
#include "../transport_error.h"
#include "sent_packet.h"

namespace utils {
    namespace dnet::quic::ack {

        struct Congestion;

        struct CongestionEventHandlers {
            std::shared_ptr<void> controller;
            error::Error (*congestion_event_cb)(std::shared_ptr<void>& arg, Congestion*);
            error::Error (*ack_window_cb)(std::shared_ptr<void>& arg, Congestion*, SentPacket*);
            error::Error (*persistent_congestion_cb)(std::shared_ptr<void>& arg, Congestion*, RemovedPackets*);

            void (*reset_cb)(std::shared_ptr<void>& arg, Congestion*);

            std::weak_ptr<void> conn;
            bool (*flow_control_limit_cb)(std::shared_ptr<void>& conn);
            error::Error (*send_one_packet_cb)(std::shared_ptr<void>& conn);

            void reset(Congestion* c) {
                if (reset_cb) {
                    reset_cb(controller, c);
                }
            }

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
                if (send_one_packet_cb) {
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
            std::uint64_t max_udp_payload_size = 0;
            std::uint64_t initial_factor = default_window_initial_factor;
            std::uint64_t minimum_factor = default_window_minimum_factor;

            // controler value
            std::uint64_t congestion_window = 0;
            std::uint64_t bytes_in_flight = 0;
            time::Time congestion_recovery_start_time = 0;

            CongestionEventHandlers handlers;

            error::Error set_max_udp_payload_size(std::uint64_t size) {
                if (size < max_udp_payload_size) {
                    return QUICError{.msg = "smaller max udp payload size"};
                }
                const bool was_min = min_window() == congestion_window;
                max_udp_payload_size = size;
                if (was_min) {
                    congestion_window = min_window();
                }
                return error::none;
            }

            void init(std::uint64_t max_datagram_size) {
                max_udp_payload_size = max_datagram_size;
                congestion_window = initial_factor * max_udp_payload_size;
                bytes_in_flight = 0;
                congestion_recovery_start_time = 0;
                handlers.reset(this);
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
                        if (!packet.is_mtu_probe) {  // MTU Probe packet loss MUST NOT affect congestion control
                            sent_time_of_last_loss = max_(sent_time_of_last_loss, packet.time_sent.value);
                        }
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

            std::uint64_t min_window() const {
                return minimum_factor * max_udp_payload_size;
            }

            // bandwidth (bytes/s)
            std::uint64_t bandwidth(const RTT& rtt) const {
                if (rtt.smoothed_rtt == 0) {
                    return ~std::uint64_t(0);
                }
                return congestion_window / rtt.smoothed_rtt;
            }
        };

    }  // namespace dnet::quic::ack
}  // namespace utils

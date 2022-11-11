/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// congestion - congestion control for QUIC
#pragma once
#include "rtt.h"
#include "../../closure.h"
#include "../transport_error.h"

namespace utils {
    namespace dnet {
        namespace quic::ack {
            struct Congestion;

            struct CongestionEventHandlers {
                closure::Closure<TransportError, Congestion*> congestion_event;
                closure::Closure<TransportError, Congestion*, SentPacket*> on_ack;
                closure::Closure<TransportError> maybeSendOnePacket;
                closure::Closure<TransportError, Congestion*, RemovedPackets&> handle_persistent_congestion;
                closure::Closure<bool> is_on_app_or_flow_control_limit;
            };

            struct Congestion {
                // config value
                size_t max_datagram_size = 0;
                size_t initial_window = 0;
                size_t minimum_window = 0;

                // controler value
                size_t congestion_window = 0;
                size_t bytes_in_flight = 0;
                time_t congestion_recovery_start_time = 0;

                CongestionEventHandlers handlers;

                void reset() {
                    congestion_window = initial_window;
                    bytes_in_flight = 0;
                    congestion_recovery_start_time = 0;
                }

                void on_packet_sent(size_t sent_bytes) {
                    bytes_in_flight += sent_bytes;
                }

                bool in_congestion_recovery(time_t time) {
                    return time <= congestion_recovery_start_time;
                }

                TransportError on_congestion_event(Clock& clock, time_t time_sent) {
                    if (in_congestion_recovery(time_sent)) {
                        return TransportError::NO_ERROR;
                    }
                    congestion_recovery_start_time = clock.now();
                    if (handlers.congestion_event) {
                        auto err = handlers.congestion_event(this);
                        if (err != TransportError::NO_ERROR) {
                            return err;
                        }
                    }
                    if (handlers.maybeSendOnePacket) {
                        return handlers.maybeSendOnePacket();
                    }
                    return TransportError::NO_ERROR;
                }

                TransportError on_packets_ack(RemovedPackets& acked) {
                    for (auto& p : acked) {
                        auto err = on_packet_ack(p);
                        if (err != TransportError::NO_ERROR) {
                            return err;
                        }
                    }
                    return TransportError::NO_ERROR;
                }

                TransportError on_packet_ack(SentPacket& acked) {
                    if (!acked.in_flight) {
                        return TransportError::NO_ERROR;
                    }
                    bytes_in_flight -= acked.sent_bytes;
                    if (handlers.is_on_app_or_flow_control_limit &&
                        handlers.is_on_app_or_flow_control_limit()) {
                        return TransportError::NO_ERROR;
                    }
                    if (in_congestion_recovery(acked.time_sent)) {
                        return TransportError::NO_ERROR;
                    }
                    if (handlers.on_ack) {
                        return handlers.on_ack(this, &acked);
                    }
                    return TransportError::NO_ERROR;
                }

                TransportError on_packets_lost(Clock& clock, RTT& rtt, RemovedPackets& packets) {
                    time_t sent_time_of_last_loss = 0;
                    for (auto& packet : packets) {
                        if (packet.in_flight) {
                            bytes_in_flight -= packet.sent_bytes;
                            sent_time_of_last_loss = max_(sent_time_of_last_loss, packet.time_sent);
                        }
                    }
                    if (sent_time_of_last_loss != 0) {
                        auto err = on_congestion_event(clock, sent_time_of_last_loss);
                        if (err != TransportError::NO_ERROR) {
                            return err;
                        }
                    }
                    if (rtt.first_ack_sample == 0) {
                        return TransportError::NO_ERROR;
                    }
                    if (handlers.handle_persistent_congestion) {
                        RemovedPackets pc_lost;
                        for (auto& lost : packets) {
                            if (lost.time_sent > rtt.first_ack_sample) {
                                pc_lost.push_back(std::move(lost));
                            }
                        }
                        return handlers.handle_persistent_congestion(this, pc_lost);
                    }
                    return TransportError::NO_ERROR;
                }
            };

            struct NewRenoCC {
                size_t persistent_congestion_threshold = 3;
                time_t persistent_duration(Clock& clock, RTT& rtt) {
                    return rtt.smoothed_rtt + max_(rtt.rttvar << 2, clock.granularity) *
                                                  persistent_congestion_threshold;
                }
                size_t sstresh = 0;
                size_t loss_reduction_factor = 0;
            };

            struct Pacer {
                utime_t N = 1;
                time_t rate(Congestion& c, RTT& rtt) {
                    if (rtt.smoothed_rtt <= 0) {
                        return -1;
                    }
                    return N * c.congestion_window / rtt.smoothed_rtt;
                }

                time_t interval(size_t packet_size, Congestion& c, RTT& rtt) {
                    if (N == 0 || c.congestion_window == 0) {
                        return -1;
                    }
                    return (rtt.smoothed_rtt * packet_size / c.congestion_window) / N;
                }
            };
        }  // namespace quic::ack
    }      // namespace dnet
}  // namespace utils

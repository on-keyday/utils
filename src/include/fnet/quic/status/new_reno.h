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
        struct NewReno : NullAlgorithm {
            size_t ssthresh = ~0;
            Ratio loss_reduction_factor{1, 2};

            constexpr void on_packet_ack(WindowModifier& modif, std::uint64_t sent_bytes, time::Time time_sent) {
                if (modif.get_window() < ssthresh) {
                    modif.update(modif.get_window() + sent_bytes);
                }
                else {
                    modif.update(modif.max_payload() * sent_bytes / modif.get_window());
                }
            }

            constexpr void on_congestion_event(WindowModifier& modif, time::Time time_sent) {
                ssthresh = loss_reduction_factor.num * modif.get_window() / loss_reduction_factor.den;
                modif.update(max_(ssthresh, modif.min_window()));
            }
        };
    }  // namespace fnet::quic::status
}  // namespace futils

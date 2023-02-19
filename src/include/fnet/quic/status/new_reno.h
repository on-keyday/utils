/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "congestion.h"

namespace utils {
    namespace fnet::quic::status {
        struct NewReno : NullAlgorithm {
            size_t sstresh = ~0;
            Ratio loss_reducion_factor{1, 2};

            constexpr void on_packet_ack(WindowModifiyer& modif, std::uint64_t sent_bytes, time::Time time_sent) {
                if (modif.get_window() < sstresh) {
                    modif.update(modif.get_window() + sent_bytes);
                }
                else {
                    modif.update(modif.max_payload() * sent_bytes / modif.get_window());
                }
            }

            constexpr void on_congestion_event(WindowModifiyer& modif, time::Time time_sent) {
                sstresh = loss_reducion_factor.num * modif.get_window() / loss_reducion_factor.den;
                modif.update(max_(sstresh, modif.min_window()));
            }
        };
    }  // namespace fnet::quic::status
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "config.h"
#include <random>
#include <chrono>

namespace utils {
    namespace fnet::quic::context {
        Config use_default(tls::TLSConfig&& tls) {
            Config config;
            config.tls_config = std::move(tls);
            config.connid_parameters.random = utils::fnet::quic::connid::Random{
                nullptr, [](std::shared_ptr<void>&, utils::view::wvec data) {
                    std::random_device dev;
                    std::uniform_int_distribution uni(0, 255);
                    for (auto& d : data) {
                        d = uni(dev);
                    }
                    return true;
                }};
            config.internal_parameters.clock = quic::time::Clock{
                nullptr,
                1,
                [](void*) {
                    return quic::time::Time(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                },
            };
            config.internal_parameters.handshake_idle_timeout = 10000;
            config.transport_parameters.max_idle_timeout = 30000;
            config.transport_parameters.initial_max_streams_uni = 100;
            config.transport_parameters.initial_max_streams_bidi = 100;
            config.transport_parameters.initial_max_data = 1000000;
            config.transport_parameters.initial_max_stream_data_uni = 100000;
            config.transport_parameters.initial_max_stream_data_bidi_local = 100000;
            config.transport_parameters.initial_max_stream_data_bidi_remote = 100000;
            config.internal_parameters.ping_duration = 15000;
            return config;
        }
    }  // namespace fnet::quic::context
}  // namespace utils

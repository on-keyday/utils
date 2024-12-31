/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "config.h"
#include "context.h"
#include <random>
#include <chrono>

namespace futils {
    namespace fnet::quic::context {
        inline bool default_gen_random(std::shared_ptr<void>&, futils::view::wvec data, futils::fnet::quic::connid::RandomUsage) {
            std::random_device dev;
            std::uniform_int_distribution uni(0, 255);
            for (auto& d : data) {
                d = uni(dev);
            }
            return true;
        }

        inline Config use_default_config(tls::TLSConfig&& tls, log::ConnLogger log = {}) {
            Config config;
            config.tls_config = std::move(tls);
            config.connid_parameters.random = futils::fnet::quic::connid::Random{
                nullptr,
                default_gen_random,
            };
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
            config.logger = log;
            return config;
        }

        template <class TConfig>
        std::shared_ptr<Context<TConfig>> use_default_context(tls::TLSConfig&& c, log::ConnLogger log = {}) {
            auto alc = std::allocate_shared<Context<TConfig>>(glheap_allocator<Context<TConfig>>{});
            auto conf = use_default_config(std::move(c), log);
            if (!alc->init(std::move(conf))) {
                return nullptr;
            }
            return alc;
        }
    }  // namespace fnet::quic::context
}  // namespace futils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// config - QUIC config
#pragma once
#include "time.h"
#include "transport_parameter/transport_param.h"
#include <cstddef>
#include "../dll/allocator.h"
#include <vector>
#include "../closure.h"
#include <random>

namespace utils {
    namespace dnet {
        namespace quic {
            struct Config {
                struct {
                    void* now_ctx = nullptr;
                    time::time_t (*now)(void*) = nullptr;
                    time::time_t msec = 0;
                    struct {
                        time::time_t num = 9;
                        time::time_t den = 8;
                    } time_threshold;
                    size_t packet_threshold = 3;
                    time::time_t initialRTT = 0;
                } ack_handler;

                struct {
                    trsparam::DefinedTransportParams params;
                    std::vector<trsparam::TransportParameter, glheap_allocator<trsparam::TransportParameter>> custom;
                } transport_parameter;

                closure::Closure<std::uint32_t> gen_random;
            };

            inline Config default_config() {
                Config conf;
                conf.ack_handler.now = time::default_now;
                conf.ack_handler.msec = time::millisec;
                conf.ack_handler.initialRTT = 333 * time::millisec;
                conf.gen_random = closure::Closure<std::uint32_t>::create<std::random_device>([](std::random_device& dev) {
                    return std::uint32_t(dev());
                });
                return conf;
            }
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// mtu - datagram limit
#pragma once
#include <cstdint>
#include "../ack/ack_lost_record.h"

namespace utils {
    namespace dnet {
        namespace quic::mtu {
            // this is for explaining
            constexpr std::uint64_t least_ip_packet_size = 1280;
            constexpr std::uint64_t initial_udp_datagram_size_limit = 1200;
            constexpr bool path_is_reject_quic(std::uint64_t datagram_limit) noexcept {
                return datagram_limit < initial_udp_datagram_size_limit;
            }

            struct PathMTU {
                std::uint64_t path_datagram_size = initial_udp_datagram_size_limit;
                std::uint64_t transport_param_value = 0;
                bool transport_param_set = false;
                std::shared_ptr<ack::ACKLostRecord> mtu_wait;

                bool apply_transport_param(std::uint64_t max_udp_payload_size) {
                    if (path_is_reject_quic(max_udp_payload_size)) {
                        return false;
                    }
                    transport_param_value = max_udp_payload_size;
                    transport_param_set = true;
                    return true;
                }

                std::uint64_t current_datagram_size() {
                    if (transport_param_set) {
                        return transport_param_value < path_datagram_size ? transport_param_value : path_datagram_size;
                    }
                    return path_datagram_size;
                }
            };
        }  // namespace quic::mtu
    }      // namespace dnet
}  // namespace utils

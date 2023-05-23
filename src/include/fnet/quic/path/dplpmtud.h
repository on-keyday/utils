/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../time.h"
#include "../ack/ack_lost_record.h"
#include "config.h"

namespace utils {
    namespace fnet::quic::path {
        enum class State {
            disabled,
            base,
            searching,
            error,
            search_complete,
        };

        struct BinarySearcher {
           private:
            std::uint64_t high = 0;
            std::uint64_t mid = 0;
            std::uint64_t low = 0;
            std::uint64_t accuracy = 1;
            bool high_updated = false;

            constexpr bool internal_complete() const noexcept {
                return (high - low) <= accuracy;
            }

            constexpr void update_next() noexcept {
                if (internal_complete() && !high_updated) {
                    if (mid != high) {
                        mid = high;
                        return;
                    }
                    high_updated = true;
                    return;
                }
                mid = low + ((high - low) >> 1);  // (high+low)/2
            }

           public:
            constexpr BinarySearcher() = default;

            // set low, high, and accurary
            constexpr bool set(size_t l, size_t h, size_t ac = 1) noexcept {
                if (l > h) {
                    return false;
                }
                low = l;
                high = h;
                accuracy = ac;
                high_updated = false;
                update_next();
                return true;
            }

            constexpr bool complete() const noexcept {
                return internal_complete() && high_updated;
            }

            constexpr size_t get_next() const noexcept {
                return mid;
            }

            constexpr void on_ack() noexcept {
                low = mid;
                update_next();
            }

            constexpr void on_lost() noexcept {
                high = mid;
                high_updated = true;
                update_next();
            }
        };

        // this is for explaining
        constexpr std::uint64_t least_ip_packet_size = 1280;
        constexpr std::uint64_t initial_udp_datagram_size = 1200;
        constexpr bool path_rejects_quic(std::uint64_t datagram_limit) noexcept {
            return datagram_limit < initial_udp_datagram_size;
        }

        struct MTU {
           private:
            State state = State::disabled;
            Config config;
            size_t current_payload_size = 0;
            size_t probe_count = 0;
            BinarySearcher bin_search;
            std::shared_ptr<ack::ACKLostRecord> wait;
            std::uint64_t transport_param_value = 0;
            bool transport_param_set = false;

           public:
            constexpr void reset(Config conf) {
                config = conf;
                if (path_rejects_quic(config.base_plpmtu)) {
                    config.base_plpmtu = initial_udp_datagram_size;
                }
                if (path_rejects_quic(config.min_plpmtu)) {
                    config.min_plpmtu = initial_udp_datagram_size;
                }
                if (path_rejects_quic(config.max_plpmtu)) {
                    config.max_plpmtu = initial_udp_datagram_size;
                }
                current_payload_size = conf.base_plpmtu;
                state = State::disabled;
                transport_param_set = false;
                transport_param_value = 0;
            }

            constexpr bool on_transport_parameter_received(std::uint64_t value) {
                if (path_rejects_quic(value)) {
                    return false;
                }
                transport_param_value = value;
                transport_param_set = true;
                return true;
            }

            constexpr void on_handshake_confirmed() {
                state = State::searching;
                const auto max_mtu = transport_param_value < config.max_plpmtu ? transport_param_value : config.max_plpmtu;
                bin_search.set(config.base_plpmtu, max_mtu);
            }

            constexpr std::pair<size_t, bool> probe_required(auto&& observer_vec) {
                if (state != State::searching) {
                    return {0, false};
                }
                if (wait) {
                    if (wait->is_lost()) {
                        probe_count++;
                        if (probe_count == config.max_probes) {
                            bin_search.on_lost();
                            probe_count = 0;
                        }
                    }
                    else if (wait->is_ack()) {
                        ack::put_ack_wait(std::move(wait));
                        current_payload_size = bin_search.get_next();  // current
                        bin_search.on_ack();
                    }
                    else {
                        return {0, false};  // waiting now
                    }
                }
                if (bin_search.complete()) {
                    wait = nullptr;
                    state = State::search_complete;
                    return {0, false};  // already done
                }
                wait = ack::make_ack_wait();
                observer_vec.push_back(wait);
                const auto next_step = bin_search.get_next();
                return {next_step, true};
            }

            constexpr std::uint64_t current_datagram_size() const {
                return current_payload_size;
            }
        };

        namespace test {
            constexpr bool check_binary_search() {
                BinarySearcher s;
                auto search = [&](size_t low, size_t high, size_t step, size_t mtu) {
                    s.set(low, high);
                    size_t i = 0;
                    while (i < step && !s.complete()) {
                        if (s.get_next() > mtu) {
                            s.on_lost();
                        }
                        else {
                            s.on_ack();
                        }
                        i++;
                    }
                };
                search(1200, 1500, 8, 1350);
                if (!s.complete()) {
                    return false;
                }
                search(1200, 1500, 10, 1500);
                if (!s.complete()) {
                    return false;
                }
                return true;
            }

            static_assert(check_binary_search());
        }  // namespace test
    }      // namespace fnet::quic::path
}  // namespace utils

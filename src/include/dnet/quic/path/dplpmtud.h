/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../time.h"
#include "../ack/ack_lost_record.h"

namespace utils {
    namespace dnet::quic::path {
        enum class State {
            disabled,
            base,
            searching,
            error,
            search_complete,
        };

        struct Timers {
            time::Timer probe;
            time::Timer pmtu_raise;
            time::Timer confirmation;
        };

        // default for QUIC
        struct Config {
            size_t max_probes = 3;
            size_t min_plpmtu = 1200;
            size_t max_plpmtu = 0xffff;
            size_t base_plpmtu = 1200;
        };

        struct BinarySearcher {
           private:
            size_t high = 0;
            size_t mid = 0;
            size_t low = 0;
            size_t accuracy = 1;
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

        struct Status {
            State state = State::disabled;
            Config config;
            Timers timers;
            size_t plp_mtu = 0;
            size_t probed_size = 0;
            size_t probe_count = 0;
            BinarySearcher bin_search;
            std::shared_ptr<ack::ACKLostRecord> wait;

            constexpr void set_handshake_confirmed() {
                state = State::searching;
                bin_search.set(config.base_plpmtu, config.max_plpmtu);
            }

            constexpr std::pair<size_t, bool> probe_required() {
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
                if (wait) {
                    wait->wait();
                }
                else {
                    wait = ack::make_ack_wait();
                }
                const auto next_step = bin_search.get_next();
                return {next_step, true};
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
    }      // namespace dnet::quic::path
}  // namespace utils

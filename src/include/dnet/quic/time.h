/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// time - QUIC time
#pragma once
#include <chrono>

namespace utils {
    namespace dnet {
        namespace quic {
            namespace time {
                using time_t = std::int64_t;

                inline time_t default_now(void*) {
                    return std::chrono::steady_clock::now().time_since_epoch().count();
                }

                constexpr time_t get_default_1ms() {
                    using namespace std::chrono_literals;
                    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(1ms).count();
                }
                constexpr time_t millisec = get_default_1ms();
            }  // namespace time

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

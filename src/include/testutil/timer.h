/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// timer - timer for test
#pragma once
#include <chrono>

namespace utils {
    namespace test {
        struct Timer {
            std::chrono::system_clock::time_point start;
            Timer() {
                reset();
            }

            auto get_now() {
                return std::chrono::system_clock::now();
            }

            void reset() {
                start = get_now();
            }

            template <class Dur = std::chrono::milliseconds>
            auto delta() {
                return std::chrono::duration_cast<Dur>(get_now() - start);
            }

            template <class Dur = std::chrono::milliseconds>
            auto next_step() {
                auto d = delta<Dur>();
                reset();
                return d;
            }
        };
    }  // namespace test
}  // namespace utils

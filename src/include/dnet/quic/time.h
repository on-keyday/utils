/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// time - QUIC time
#pragma once

namespace utils {
    namespace dnet::quic::time {

        using time_t = std::int64_t;
        using utime_t = std::uint64_t;

        struct Time {
            std::int64_t value = 0;

            constexpr Time() noexcept = default;

            constexpr Time(std::int64_t v)
                : value(v) {}

            constexpr operator std::int64_t() const noexcept {
                return value;
            }

            constexpr bool valid() const noexcept {
                return value >= 0;
            }
        };

        constexpr auto invalid = Time{-1};

        struct Clock {
            void* ptr = nullptr;
            Time (*now_fn)(void*) = nullptr;
            time::time_t granularity = 1000000;  // millisec
            Time now() {
                return now_fn ? now_fn(ptr) : invalid;
            }
        };

        struct Timer {
           private:
            time::Time deadline = invalid;

           public:
            void set_deadline(time::Time time) {
                if (!time.valid()) {
                    deadline = invalid;
                }
                else {
                    deadline = time;
                }
            }

            void cancel() {
                deadline = invalid;
            }

            bool timeout(Clock& clock) const {
                return deadline != invalid && deadline < clock.now();
            }

            void set_timeout(Clock& clock, time::utime_t delta) {
                set_deadline(clock.now() + delta);
            }
        };

    }  // namespace dnet::quic::time

}  // namespace utils

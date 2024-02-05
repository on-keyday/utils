/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// time - QUIC time
#pragma once
#include <cstdint>
#include <type_traits>

namespace futils {
    namespace fnet::quic::time {

        using time_t = std::int64_t;
        using utime_t = std::uint64_t;

        struct Time {
           private:
            std::int64_t value = 0;

           public:
            constexpr Time() noexcept = default;

            constexpr Time(std::int64_t v)
                : value(v) {}

            constexpr operator std::int64_t() const noexcept {
                return value;
            }

            constexpr std::int64_t to_int() const noexcept {
                return value;
            }

            constexpr bool valid() const noexcept {
                return value >= 0;
            }
        };

        constexpr auto invalid = Time{-1};

        struct Clock {
           private:
            void* ptr = nullptr;
            Time (*now_fn)(void*) = nullptr;
            time::utime_t millisecond = 1;  // millisec

           public:
            constexpr Clock() = default;

            constexpr Clock(void* p, time::time_t millisec, Time (*fn)(void*))
                : ptr(p), now_fn(fn), millisecond(millisec) {}

            template <class T>
            constexpr Clock(std::type_identity_t<T>* p, time::utime_t millisec, Time (*fn)(T*))
                : Clock(p, millisec, reinterpret_cast<Time (*)(void*)>(fn)) {}

            constexpr Time now() const {
                return now_fn ? now_fn(ptr) : invalid;
            }

            constexpr time::time_t to_clock_granularity(time::time_t millisec) const noexcept {
                return millisec * millisecond;
            }

            constexpr time::time_t to_millisec(time::time_t t) const noexcept {
                return t / millisecond;
            }

            constexpr time::time_t to_nanosec(time::time_t t) const noexcept {
                const auto ratio = 1000000;
                return t * ratio / millisecond;
            }
        };

        struct Timer {
           private:
            time::Time deadline = invalid;

           public:
            constexpr void set_deadline(time::Time time) {
                if (!time.valid()) {
                    deadline = invalid;
                }
                else {
                    deadline = time;
                }
            }

            constexpr bool not_working() const {
                return !deadline.valid();
            }

            constexpr time::Time get_deadline() const {
                return deadline;
            }

            constexpr void cancel() {
                deadline = invalid;
            }

            constexpr bool timeout(const Clock& clock) const {
                return deadline != invalid && deadline < clock.now();
            }

            constexpr void set_timeout(const Clock& clock, time::utime_t delta) {
                set_deadline(clock.now() + delta);
            }
        };

    }  // namespace fnet::quic::time

}  // namespace futils

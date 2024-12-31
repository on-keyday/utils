/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <chrono>
#include "time.h"

namespace futils::timer {

    struct Clock {
       private:
        Time (*now_)(const Clock*) = nullptr;

       protected:
        constexpr Clock(Time (*now)(const Clock*))
            : now_(now) {}

       public:
        constexpr Time now() const {
            return now_(this);
        }
    };

    struct SystemClock : Clock {
        constexpr SystemClock()
            : Clock([](const Clock*) {
                  auto now = std::chrono::system_clock::now();
                  auto sec = std::chrono::time_point_cast<std::chrono::seconds>(
                                 now)
                                 .time_since_epoch()
                                 .count();
                  auto nsec =
                      std::chrono::time_point_cast<std::chrono::nanoseconds>(
                          now)
                          .time_since_epoch()
                          .count();
                  return Time{static_cast<std::uint64_t>(sec), static_cast<std::uint32_t>(nsec % 1000000000)};
              }) {}
    };

    struct SteadyClock : Clock {
        constexpr SteadyClock()
            : Clock([](const Clock*) {
                  auto now = std::chrono::steady_clock::now();
                  auto sec = std::chrono::time_point_cast<std::chrono::seconds>(
                                 now)
                                 .time_since_epoch()
                                 .count();
                  auto nsec =
                      std::chrono::time_point_cast<std::chrono::nanoseconds>(
                          now)
                          .time_since_epoch()
                          .count();
                  return Time{static_cast<std::uint64_t>(sec), static_cast<std::uint32_t>(nsec % 1000000000)};
              }) {}
    };

#if __cpp_lib_chrono >= 201907L
    struct UTCClock : Clock {
        constexpr UTCClock()
            : Clock([](const Clock*) {
                  auto now = std::chrono::utc_clock::now();
                  auto sec = std::chrono::time_point_cast<std::chrono::seconds>(
                                 now)
                                 .time_since_epoch()
                                 .count();
                  auto nsec =
                      std::chrono::time_point_cast<std::chrono::nanoseconds>(
                          now)
                          .time_since_epoch()
                          .count();
                  return Time{static_cast<std::uint64_t>(sec), static_cast<std::uint32_t>(nsec % 1000000000)};
              }) {}
    };
    constexpr auto utc_clock = UTCClock();
#else
    constexpr auto utc_clock = SystemClock();
#endif
    constexpr auto std_clock = SystemClock();
    constexpr auto steady_clock = SteadyClock();

    struct FixedClock : Clock {
       private:
        Time time;

       public:
        constexpr FixedClock(Time time)
            : Clock([](const Clock* self) {
                  return static_cast<const FixedClock*>(self)->time;
              }),
              time(time) {}
    };

    struct StubClock : Clock {
        std::uint64_t timer_sec = 0;
        std::uint32_t timer_nsec = 0;
        std::uint64_t timer_delta_sec = 0;
        std::uint32_t timer_delta_nsec = 0;

        constexpr StubClock()
            : Clock([](const Clock* self) {
                  auto timer = const_cast<StubClock*>(static_cast<const StubClock*>(self));
                  auto now = Time{timer->timer_sec, timer->timer_nsec};
                  timer->timer_sec += timer->timer_delta_sec;
                  timer->timer_nsec += timer->timer_delta_nsec;
                  timer->timer_sec += timer->timer_nsec / 1000000000;
                  timer->timer_nsec %= 1000000000;
                  return now;
              }) {}
    };

}  // namespace futils::timer

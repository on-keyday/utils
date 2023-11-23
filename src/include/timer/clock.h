/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <compare>
#include <chrono>

namespace utils::timer {
    struct Time {
       private:
        std::uint64_t second = 0;
        std::uint32_t nanosecond = 0;

       public:
        constexpr Time(std::uint64_t second, std::uint32_t nanosecond)
            : second(second), nanosecond(nanosecond) {}

        constexpr std::uint64_t to_millisecond() const {
            return second * 1000 + nanosecond / 1000000;
        }

        constexpr std::uint64_t to_microsecond() const {
            return second * 1000000 + nanosecond / 1000;
        }

        constexpr std::uint64_t to_nanosecond() const {
            return second * 1000000000 + nanosecond;
        }

        constexpr friend bool operator==(const Time& left, const Time& right) {
            return left.second == right.second &&
                   left.nanosecond == right.nanosecond;
        }

        constexpr friend auto operator<=>(const Time& left, const Time& right) {
            if (left.second != right.second) {
                return left.second <=> right.second;
            }
            return left.nanosecond <=> right.nanosecond;
        }

        constexpr friend Time operator+(const Time& left, const Time& right) {
            auto nsec = left.nanosecond + right.nanosecond;
            auto sec = left.second + right.second + nsec / 1000000000;
            nsec %= 1000000000;
            return {sec, nsec};
        }
    };

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
                  return Time{sec, static_cast<std::uint32_t>(nsec % 1000000000)};
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
                  return Time{sec, static_cast<std::uint32_t>(nsec % 1000000000)};
              }) {}
    };

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

}  // namespace utils::timer

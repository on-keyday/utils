/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../varint.h"

namespace utils {
    namespace fnet::quic ::stream::core {
        struct Limiter {
           private:
            std::uint64_t limit = 0;
            std::uint64_t used = 0;

           public:
            constexpr Limiter() = default;
            constexpr std::uint64_t avail_size() const {
                return limit > used ? limit - used : 0;
            }

            constexpr bool on_limit(std::uint64_t delta) const {
                return avail_size() < delta;
            }

            // use this for first time or transport parameter applying.
            // use update_limit instead later time
            constexpr bool set_limit(std::uint64_t new_limit) {
                if (new_limit >= varint::border(8)) {
                    return false;
                }
                limit = new_limit;
                return true;
            }

            constexpr bool update_limit(std::uint64_t new_limit) {
                if (new_limit >= varint::border(8)) {
                    return false;
                }
                if (new_limit > limit) {
                    limit = new_limit;
                    return true;
                }
                return false;
            }

            constexpr bool use(std::uint64_t s) {
                if (on_limit(s)) {
                    return false;
                }
                used += s;
                return true;
            }

            constexpr bool update_use(std::uint64_t new_used) {
                if (limit < new_used) {
                    return false;
                }
                if (new_used > used) {
                    used = new_used;
                }
                return true;
            }

            constexpr std::uint64_t current_limit() const {
                return limit;
            }

            constexpr std::uint64_t current_usage() const {
                return used;
            }
        };
    }  // namespace fnet::quic::stream::core
}  // namespace utils
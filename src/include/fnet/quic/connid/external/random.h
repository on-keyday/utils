/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>
#include <view/iovec.h>

namespace utils {
    namespace fnet::quic::connid {

        enum class RandomUsage : byte {
            original_dst_id,
            retry_id,
            connection_id,
            stateless_reset_token,
            id_change_duration,
            path_challenge,
        };

        struct Random {
           private:
            std::shared_ptr<void> arg;
            bool (*user_gen_random)(std::shared_ptr<void>& arg, view::wvec random, RandomUsage usage) = nullptr;

           public:
            constexpr Random() = default;
            Random(std::shared_ptr<void> arg, bool (*gen_rand)(std::shared_ptr<void>&, view::wvec random, RandomUsage))
                : arg(std::move(arg)), user_gen_random(gen_rand) {}
            bool gen_random(view::wvec random, RandomUsage usage) {
                if (!user_gen_random) {
                    return false;
                }
                return user_gen_random(arg, random, usage);
            }
            constexpr bool valid() const {
                return user_gen_random != nullptr;
            }
        };

    }  // namespace fnet::quic::connid
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>
#include <memory>
#include "../../../error.h"
#include "random.h"

namespace utils {
    namespace fnet::quic::connid {
        struct ConnIDGenerator {
            error::Error (*gen_calllback)(std::shared_ptr<void>& arg, Random& rand, std::uint32_t version, byte len, view::wvec connID, view::wvec statless_reset) = nullptr;
            std::shared_ptr<void> arg;

            error::Error generate(Random& rand, std::uint32_t version, byte len, view::wvec connID, view::wvec stateless_reset) {
                if (gen_calllback) {
                    return gen_calllback(arg, rand, version, len, connID, stateless_reset);
                }
                return error::Error("no connID generator");
            }
        };

        inline error::Error default_generator(std::shared_ptr<void>& arg, Random& rand, std::uint32_t version, byte len, view::wvec connID, view::wvec stateless_reset) {
            if (!rand.valid()) {
                return error::Error("invalid random");
            }
            if (!rand.gen_random(connID) ||
                !rand.gen_random(stateless_reset)) {
                return error::Error("gen_random failed");
            }
            return error::none;
        }

    }  // namespace fnet::quic::connid
}  // namespace utils

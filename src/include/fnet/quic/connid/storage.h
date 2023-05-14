/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../storage.h"

namespace utils {
    namespace fnet::quic::connid {
        using StatelessResetToken = byte[16];

        struct ConnID {
            std::int64_t seq = -1;
            view::rvec id;
            view::rvec stateless_reset_token;
        };

        struct IDStorage {
            std::int64_t seq = -1;
            storage id;
            StatelessResetToken stateless_reset_token;

            ConnID to_ConnID() const {
                return ConnID{
                    .seq = seq,
                    .id = id,
                    .stateless_reset_token = stateless_reset_token,
                };
            }
        };

        constexpr StatelessResetToken null_stateless_reset{};

        struct CloseID {
            storage id;
            StatelessResetToken token;
        };

    }  // namespace fnet::quic::connid
}  // namespace utils

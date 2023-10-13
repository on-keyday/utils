/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../view/iovec.h"

namespace utils {
    namespace fnet::quic::stream {
        struct Fragment {
            std::uint64_t offset;
            view::rvec fragment;
            bool fin = false;
        };

    }  // namespace fnet::quic::stream
}  // namespace utils

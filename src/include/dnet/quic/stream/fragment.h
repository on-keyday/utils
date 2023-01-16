/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../view/iovec.h"

namespace utils {
    namespace dnet::quic::stream {
        struct Fragment {
            size_t offset;
            view::rvec fragment;
            bool fin = false;
        };

    }  // namespace dnet::quic::stream
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace utils {
    namespace fnet::http3 {
        template <class QpackConfig, class QuicConfig>
        struct TypeConfig {
            using qpack_config = QpackConfig;
            using quic_config = QuicConfig;
        };
    }  // namespace fnet::http3
}  // namespace utils

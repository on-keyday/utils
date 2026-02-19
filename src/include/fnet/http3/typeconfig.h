/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils {
    namespace fnet::http3 {
        template <class QpackConfig, class QuicConfig>
        struct TypeConfig {
            using qpack_config = QpackConfig;
            using quic_config = QuicConfig;
            using conn_lock = typename QuicConfig::conn_lock;
            using reader_lock = typename QuicConfig::recv_stream_lock;
        };
    }  // namespace fnet::http3
}  // namespace futils

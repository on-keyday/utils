/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ack_generate - ack generator
#pragma once
#include "quic_contexts.h"

namespace utils {
    namespace dnet {
        namespace quic {
            void generate_ack(QUICContexts* q);
        }
    }  // namespace dnet
}  // namespace utils

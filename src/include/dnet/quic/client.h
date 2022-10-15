/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// client - QUIC client
#pragma once
#include "bytelen.h"

namespace utils {
    namespace dnet {
        namespace quic {
            enum class QUICState {
                Init,
                Sending_IntialPacket,
            };

            struct StateMacine {
                QUICState state = QUICState::Init;
            };
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

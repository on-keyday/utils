/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// gen_frames - generate frames
#pragma once
#include "quic_contexts.h"

namespace utils {
    namespace dnet {
        namespace quic::handler {
            error::Error generate_frames(QUICContexts* q);

            error::Error on_frame_render(QUICContexts* q, const frame::Frame& f, size_t packet_number);
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

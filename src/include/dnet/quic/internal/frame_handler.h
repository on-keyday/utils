/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../frame/frame_make.h"
#include "quic_contexts.h"

namespace utils {
    namespace dnet {
        namespace quic::handler {
            bool on_ack(QUICContexts* q, frame::ACKFrame& ack, ack::PacketNumberSpace space);

            bool on_crypto(QUICContexts* q, frame::CryptoFrame& c, crypto::EncryptionLevel level);

            bool on_connection_close(QUICContexts* q, frame::ConnectionCloseFrame& c);
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
// #include "../frame/frame_make.h"
#include "quic_contexts.h"
#include "../frame/wire.h"

namespace utils {
    namespace dnet {
        namespace quic::handler {
            error::Error on_ack(QUICContexts* q, frame::ACKFrame<easy::Vec>& ack, ack::PacketNumberSpace space);

            error::Error on_crypto(QUICContexts* q, frame::CryptoFrame& c, crypto::EncryptionLevel level);

            error::Error on_connection_close(QUICContexts* q, frame::ConnectionCloseFrame& c);

            error::Error on_new_connection_id(QUICContexts* q, frame::NewConnectionIDFrame& c);

            error::Error on_streams_blocked(QUICContexts* q, frame::StreamsBlockedFrame& blocked);
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

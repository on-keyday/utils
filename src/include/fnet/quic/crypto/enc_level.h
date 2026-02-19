/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../types.h"

namespace futils {
    namespace fnet::quic::crypto {
        enum class ArgType {
            // openssl
            // cipher,read_secret,write_secret,secret_len,level is enabled
            secret,
            // boring ssl
            // cipher,write_secret,secret_len,level is enabled
            wsecret,
            // boring ssl
            // cipher,read_secret,secret_len,level is enabled
            rsecret,
            // data,len,level is enabled
            handshake_data,
            // nonthing enabled
            flush,
            // alert is enabled
            alert,
        };

        enum class EncryptionLevel {
            initial = 0,
            early_data,
            handshake,
            application,
        };

        constexpr int level_to_index(EncryptionLevel level) noexcept {
            return int(level);
        }

        constexpr EncryptionLevel type_to_level(PacketType type) noexcept {
            switch (type) {
                case PacketType::Initial:
                    return EncryptionLevel::initial;
                case PacketType::Handshake:
                    return EncryptionLevel::handshake;
                case PacketType::OneRTT:
                    return EncryptionLevel::application;
                case PacketType::ZeroRTT:
                    return EncryptionLevel::early_data;
                default:
                    break;
            }
            // undefined behaivour
            EncryptionLevel e;
            return e;
        }

    }  // namespace fnet::quic::crypto
}  // namespace futils

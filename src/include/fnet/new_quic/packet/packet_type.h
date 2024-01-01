/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/byte.h>
#include "../version.h"

namespace futils::fnet::quic::packet {

    enum class Type : byte {
        Initial,
        ZeroRTT,  // 0-RTT
        Handshake,
        Retry,
        VersionNegotiation,  // section 17.2.1
        OneRTT,              // 1-RTT

        Unknown,
        LongPacket,
        StatelessReset,
    };

    constexpr const char* to_string(Type type) noexcept {
#define MAP(code)    \
    case Type::code: \
        return #code;
        switch (type) {
            MAP(Initial)
            MAP(ZeroRTT)
            MAP(Handshake)
            MAP(Retry)
            MAP(VersionNegotiation)
            MAP(OneRTT)
            MAP(Unknown)
            MAP(LongPacket)
            MAP(StatelessReset)
            default:
                return nullptr;
        }
#undef MAP
    }

    constexpr Type long_packet_type(byte type_bit_1_to_3, Version version) {
        if (version == Version::version_negotiation) {
            return Type::VersionNegotiation;
        }
        if (version == Version::version_1) {
            switch (type_bit_1_to_3) {
                case 0:
                    return Type::Initial;
                case 1:
                    return Type::ZeroRTT;
                case 2:
                    return Type::Handshake;
                case 3:
                    return Type::Retry;
                default:
                    break;
            }
        }
        else if (version == Version::version_2) {
            switch (type_bit_1_to_3) {
                case 0:
                    return Type::Retry;
                case 1:
                    return Type::Initial;
                case 2:
                    return Type::ZeroRTT;
                case 3:
                    return Type::Handshake;
                default:
                    break;
            }
        }
        return Type::Unknown;
    }

    constexpr byte long_packet_type_bit_1_to_3(Type type, Version version) {
        if (version == Version::version_negotiation) {
            return 0;
        }
        if (version == Version::version_1) {
            switch (type) {
                case Type::Initial:
                    return 0;
                case Type::ZeroRTT:
                    return 1;
                case Type::Handshake:
                    return 2;
                case Type::Retry:
                    return 3;
                default:
                    break;
            }
        }
        else if (version == Version::version_2) {
            switch (type) {
                case Type::Retry:
                    return 0;
                case Type::Initial:
                    return 1;
                case Type::ZeroRTT:
                    return 2;
                case Type::Handshake:
                    return 3;
                default:
                    break;
            }
        }
        return -1;
    }

}  // namespace futils::fnet::quic::packet

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// types - QUIC basic types
#pragma once
#include "../byte.h"

namespace utils {
    namespace dnet {
        namespace quic {
            enum class PacketType {
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

            constexpr bool is_LongPacket(PacketType type) {
                return type == PacketType::Initial ||
                       type == PacketType::ZeroRTT ||
                       type == PacketType::Handshake ||
                       type == PacketType::Retry;
            }

            constexpr const char* to_string(PacketType type) {
#define MAP(code)          \
    case PacketType::code: \
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

            // packe_type_to_mask returns packet number byte mask for type and version
            // if succeeded returns mask else returns 0xff
            constexpr byte packet_type_to_mask(PacketType type, std::uint32_t version) {
                if (type == PacketType::OneRTT) {
                    return 0x40;
                }
                if (type == PacketType::VersionNegotiation) {
                    return 0x80;
                }
                if (version == 1) {
                    switch (type) {
                        case PacketType::Initial:
                            return 0xC0;
                        case PacketType::ZeroRTT:
                            return 0xD0;
                        case PacketType::Handshake:
                            return 0xE0;
                        case PacketType::Retry:
                            return 0xF0;
                        default:
                            break;
                    }
                }
                return 0;
            }

            constexpr PacketType long_packet_type(byte typbit_1_to_3, std::uint32_t version) {
                if (version == 0) {
                    return PacketType::VersionNegotiation;
                }
                if (version == 1) {
                    switch (typbit_1_to_3) {
                        case 0:
                            return PacketType::Initial;
                        case 1:
                            return PacketType::ZeroRTT;
                        case 2:
                            return PacketType::Handshake;
                        case 3:
                            return PacketType::Retry;
                        default:
                            break;
                    }
                }
                return PacketType::Unknown;
            }

            struct PacketFlags {
                byte value = 0;

                constexpr byte raw() const {
                    return value;
                }

                constexpr bool invalid() const {
                    return (raw() & 0x40) == 0;
                }

                constexpr bool is_short() const {
                    return (raw() & 0x80) == 0;
                }

                constexpr bool is_long() const {
                    return !is_short();
                }

                constexpr PacketType type(std::uint32_t version = 1) const {
                    if (is_short()) {
                        return PacketType::OneRTT;
                    }
                    byte long_header_type = (raw() & 0x30) >> 4;
                    return long_packet_type(long_header_type, version);
                }

                constexpr byte spec_mask() const {
                    switch (type()) {
                        case PacketType::VersionNegotiation:
                            return 0x7f;
                        case PacketType::OneRTT:
                            return 0x3f;
                        default:
                            return 0x0f;
                    }
                }

                constexpr byte flags() const {
                    return raw() & spec_mask();
                }

                constexpr byte protect_mask() const {
                    if (is_short()) {
                        return 0x1f;
                    }
                    else {
                        return 0x0f;
                    }
                }

                constexpr byte protect(byte with) const {
                    return raw() ^ (with & protect_mask());
                }

                constexpr byte packet_number_length() const {
                    return (raw() & 0x3) + 1;
                }

                constexpr byte reserved_bits() const {
                    if (is_short()) {
                        return raw() & 0x18;
                    }
                    return raw() & 0x0C;
                }

                constexpr bool spin_bit() const {
                    if (is_short()) {
                        return raw() & 0x20;
                    }
                    return false;
                }

                constexpr bool key_phase() const {
                    if (is_short()) {
                        return raw() & 0x04;
                    }
                    return false;
                }
            };

            constexpr PacketFlags make_packet_flags(std::uint32_t version, PacketType type, byte pn_length, bool spin = false, bool key_phase = false) {
                byte raw = 0;
                auto lentomask = [&] {
                    return byte(pn_length - 1);
                };
                auto valid_pnlen = [&] {
                    return 1 <= pn_length && pn_length <= 4;
                };
                auto typmask = packet_type_to_mask(type, version);
                if (typmask == 0) {
                    return {};
                }
                if (type == PacketType::Initial || type == PacketType::ZeroRTT ||
                    type == PacketType::Handshake || type == PacketType::Retry) {
                    if (!valid_pnlen()) {
                        return {};
                    }
                    raw = typmask | lentomask();
                }
                else if (type == PacketType::VersionNegotiation) {
                    raw = typmask;
                }
                else if (type == PacketType::OneRTT) {
                    if (!valid_pnlen()) {
                        return {};
                    }
                    raw = typmask | (spin ? 0x20 : 0) | (key_phase ? 0x04 : 0) | lentomask();
                }
                else {
                    return PacketFlags{0};
                }
                return PacketFlags{raw};
            }

            enum class FrameType {
                PADDING,                                 // section 19.1
                PING,                                    // section 19.2
                ACK,                                     // section 19.3
                ACK_ECN,                                 // with ECN count
                RESET_STREAM,                            // section 19.4
                STOP_SENDING,                            // section 19.5
                CRYPTO,                                  // section 19.6
                NEW_TOKEN,                               // section 19.7
                STREAM,                                  // section 19.8
                MAX_DATA = 0x10,                         // section 19.9
                MAX_STREAM_DATA,                         // section 19.10
                MAX_STREAMS,                             // section 19.11
                MAX_STREAMS_BIDI = MAX_STREAMS,          // section 19.11
                MAX_STREAMS_UNI,                         // section 19.11
                DATA_BLOCKED,                            // section 19.12
                STREAM_DATA_BLOCKED,                     // section 19.13
                STREAMS_BLOCKED,                         // section 19.14
                STREAMS_BLOCKED_BIDI = STREAMS_BLOCKED,  // section 19.14
                STREAMS_BLOCKED_UNI,                     // section 19.14
                NEW_CONNECTION_ID,                       // section 19.15
                RETIRE_CONNECTION_ID,                    // seciton 19.16
                PATH_CHALLENGE,                          // seciton 19.17
                PATH_RESPONSE,                           // seciton 19.18
                CONNECTION_CLOSE,                        // seciton 19.19
                CONNECTION_CLOSE_APP,                    // application reason section 19.19
                HANDSHAKE_DONE,                          // seciton 19.20
                DATAGRAM = 0x30,                         // rfc 9221 datagram
                DATAGRAM_LEN,
            };

            constexpr bool is_STREAM(FrameType type) {
                constexpr auto st = int(FrameType::STREAM);
                const auto ty = int(type);
                return st <= ty && ty <= (st | 0x7);
            }

            constexpr bool is_MAX_STREAMS(FrameType type) {
                return type == FrameType::MAX_STREAMS_BIDI ||
                       type == FrameType::MAX_STREAMS_UNI;
            }

            constexpr bool is_STREAMS_BLOCKED(FrameType type) {
                return type == FrameType::STREAMS_BLOCKED_BIDI ||
                       type == FrameType::STREAMS_BLOCKED_UNI;
            }

            constexpr bool is_ACK(FrameType type) {
                return type == FrameType::ACK ||
                       type == FrameType::ACK_ECN;
            }

            constexpr bool is_CONNECTION_CLOSE(FrameType type) {
                return type == FrameType::CONNECTION_CLOSE ||
                       type == FrameType::CONNECTION_CLOSE_APP;
            }

            constexpr bool is_DATAGRAM(FrameType type) {
                return type == FrameType::DATAGRAM ||
                       type == FrameType::DATAGRAM_LEN;
            }

            constexpr const char* to_string(FrameType type) {
#define MAP(code)         \
    case FrameType::code: \
        return #code;
                switch (type) {
                    MAP(PADDING)
                    MAP(PING)
                    MAP(ACK)
                    MAP(ACK_ECN)
                    MAP(RESET_STREAM)
                    MAP(STOP_SENDING)
                    MAP(CRYPTO)
                    MAP(NEW_TOKEN)
                    MAP(MAX_DATA)
                    MAP(MAX_STREAM_DATA)
                    MAP(MAX_STREAMS_BIDI)
                    MAP(MAX_STREAMS_UNI)
                    MAP(DATA_BLOCKED)
                    MAP(STREAM_DATA_BLOCKED)
                    MAP(STREAMS_BLOCKED_BIDI)
                    MAP(STREAMS_BLOCKED_UNI)
                    MAP(NEW_CONNECTION_ID)
                    MAP(RETIRE_CONNECTION_ID)
                    MAP(PATH_CHALLENGE)
                    MAP(PATH_RESPONSE)
                    MAP(CONNECTION_CLOSE)
                    MAP(CONNECTION_CLOSE_APP)
                    MAP(HANDSHAKE_DONE)
                    MAP(DATAGRAM)
                    MAP(DATAGRAM_LEN)
                    default:
                        if (is_STREAM(type)) {
                            return "STREAM";
                        }
                        return nullptr;
                }
#undef MAP
            }

            // rfc 9000 12.4. Frames and Frame Types
            // Table 3. Frame Types

            constexpr bool is_ACKEliciting(FrameType type) {
                return type != FrameType::PADDING &&
                       type != FrameType::ACK &&
                       type != FrameType::ACK_ECN &&
                       type != FrameType::CONNECTION_CLOSE &&
                       type != FrameType::CONNECTION_CLOSE_APP;
            }

            constexpr bool is_MTUProbe(FrameType type) {
                return type == FrameType::PADDING ||
                       type == FrameType::NEW_CONNECTION_ID ||
                       type == FrameType::PATH_CHALLENGE ||
                       type == FrameType::PATH_RESPONSE;
            }

            constexpr bool is_FlowControled(FrameType type) {
                return is_STREAM(type);
            }

            constexpr bool is_ByteCounted(FrameType type) {
                return type != FrameType::ACK;
            }

            constexpr bool is_InitialPacketOK(FrameType type) {
                return type == FrameType::PADDING ||
                       type == FrameType::PING ||
                       type == FrameType::ACK ||
                       type == FrameType::CRYPTO ||
                       type == FrameType::CONNECTION_CLOSE;
            }

            constexpr bool is_HandshakePacketOK(FrameType type) {
                return is_InitialPacketOK(type);
            }

            constexpr bool is_ZeroRTTPacketOK(FrameType type) {
                return type != FrameType::ACK &&
                       type != FrameType::CRYPTO &&
                       type != FrameType::NEW_TOKEN &&
                       type != FrameType::PATH_RESPONSE &&
                       type != FrameType::HANDSHAKE_DONE;
            }

            constexpr bool is_OneRTTPacketOK(FrameType type) {
                return true;  // all types are allowed
            }

            struct FrameFlags {
                size_t value = 0;

                /*
                constexpr bool valid() const {
                    return value != nullptr;
                }

                constexpr byte raw() const {
                    return value ? *value : 0;
                }
                */
                constexpr FrameFlags() = default;
                constexpr FrameFlags(size_t typ)
                    : value(typ) {}

                constexpr FrameFlags(FrameType typ)
                    : value(size_t(typ)) {}

                constexpr byte as_byte() const {
                    return value & 0xff;
                }

                constexpr FrameType type_detail() const {
                    return FrameType(value);
                }

                constexpr FrameType type() const {
                    auto ty = type_detail();
                    if (is_STREAM(ty)) {
                        return FrameType::STREAM;
                    }
                    if (is_MAX_STREAMS(ty)) {
                        return FrameType::MAX_STREAMS;
                    }
                    if (is_CONNECTION_CLOSE(ty)) {
                        return FrameType::CONNECTION_CLOSE;
                    }
                    if (is_STREAMS_BLOCKED(ty)) {
                        return FrameType::STREAMS_BLOCKED;
                    }
                    if (is_ACK(ty)) {
                        return FrameType::ACK;
                    }
                    if (is_DATAGRAM(ty)) {
                        return FrameType::DATAGRAM;
                    }
                    return ty;
                }

                constexpr bool STREAM_fin() const {
                    if (type() == FrameType::STREAM) {
                        return as_byte() & 0x01;
                    }
                    return false;
                }

                constexpr bool STREAM_len() const {
                    if (type() == FrameType::STREAM) {
                        return as_byte() & 0x02;
                    }
                    return false;
                }

                constexpr bool STREAM_off() const {
                    if (type() == FrameType::STREAM) {
                        return as_byte() & 0x04;
                    }
                    return false;
                }

                constexpr bool DATAGRAM_len() const {
                    return type_detail() == FrameType::DATAGRAM_LEN;
                }
            };

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

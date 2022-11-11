/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_util - packet utility
#pragma once
#include "../bytelen.h"
#include "packet.h"

namespace utils {
    namespace dnet {
        namespace quic::packet {
            // cb is void(PacketType,Packet&,bool err,bool valid_type) or
            // bool(PacketType,Packet&,bool err,bool valid_type)
            bool parse_packet(ByteLen& b, auto&& cb, auto&& is_stateless_reset, auto&& get_dstID_len) {
                if (!b.enough(1)) {
                    return false;
                }
                auto invoke_cb = [&](PacketType type, auto& packet, bool err, bool valid_type) {
                    if constexpr (internal::has_logical_not<decltype(cb(type, packet, err, valid_type))>) {
                        if (!cb(type, packet, err, valid_type)) {
                            return false;
                        }
                    }
                    else {
                        cb(type, packet, err, valid_type);
                    }
                    return true;
                };
                auto flags = PacketFlags{b.data};
                if (flags.is_long()) {
                    std::uint32_t version = 0;
                    if (!b.forward(1).as(version)) {
                        Packet packet;
                        packet.flags = flags;
                        invoke_cb(PacketType::Unknown, packet, true, false);
                        return false;
                    }
                    if (version == 0) {
                        VersionNegotiationPacket verneg;
                        if (!verneg.parse(b)) {
                            invoke_cb(PacketType::VersionNegotiation, verneg, true, true);
                            return false;
                        }
                        return invoke_cb(PacketType::VersionNegotiation, verneg, false, true);
                    }
                    auto type = flags.type(version);
#define PACKET(ptype, TYPE)                           \
    if (type == ptype) {                              \
        TYPE packet;                                  \
        if (!packet.parse(b)) {                       \
            invoke_cb(ptype, packet, true, true);     \
            return false;                             \
        }                                             \
        return invoke_cb(ptype, packet, false, true); \
    }
                    PACKET(PacketType::Initial, InitialPacketCipher)
                    PACKET(PacketType::ZeroRTT, ZeroRTTPacketCipher)
                    PACKET(PacketType::HandShake, HandshakePacketCipher)
                    PACKET(PacketType::Retry, RetryPacket)
                    LongPacketBase base;
                    base.flags = flags;
                    base.version = b.forward(1).resized(4);
                    invoke_cb(PacketType::LongPacket, base, true, false);
                    return false;
#undef PACKET
                }
                {
                    auto copy = b;
                    StatelessReset reset;
                    if (reset.parse(copy)) {
                        if (is_stateless_reset(reset)) {
                            b = copy;
                            return invoke_cb(PacketType::StateLessReset, reset, false, true);
                        }
                    }
                }
                OneRTTPacketCipher packet;
                if (!packet.parse(b, get_dstID_len)) {
                    invoke_cb(PacketType::OneRTT, packet, true, true);
                    return false;
                }
                return invoke_cb(PacketType::OneRTT, packet, false, true);
            }

            bool parse_packets(ByteLen& b, auto&& cb, auto&& is_stateless_reset, auto&& get_dstID_len) {
                while (b.len) {
                    if (!parse_packet(b, cb, is_stateless_reset, get_dstID_len)) {
                        return false;
                    }
                }
                return true;
            }

            struct PacketNumber {
                std::uint32_t value = 0;
                byte len = 0;
            };

            constexpr InitialPacketPlain make_initial_plain(
                WPacket& src, PacketNumber packet_number, std::uint32_t version,
                ByteLen dstConnID, ByteLen srcConnID,
                ByteLen token, ByteLen payload, size_t payload_tag_len, size_t reqlen) {
                if (!dstConnID.valid()) {
                    dstConnID = src.acquire(0);
                }
                if (!srcConnID.valid()) {
                    srcConnID = src.acquire(0);
                }
                if (!token.valid()) {
                    token = src.acquire(0);
                }
                InitialPacketPlain packet;
                packet.flags = src.packet_flags(version, PacketType::Initial, packet_number.len);
                packet.version = src.as(version);
                packet.dstIDLen = src.as_byte(dstConnID.len);
                packet.dstID = dstConnID;
                packet.srcIDLen = src.as_byte(srcConnID.len);
                packet.srcID = srcConnID;
                packet.token_length = src.qvarint(token.len);
                packet.token = token;
                const auto current_packet_len = packet.as_partial().packet_len();
                auto payload_len = payload.len;
                auto length_value = packet_number.len + payload_len + payload_tag_len;
                if (reqlen) {
                    if (reqlen < current_packet_len + length_value) {
                        return {};
                    }
                    auto padding_and_length_len = reqlen - current_packet_len - length_value;
                    bool ok = false;
                    for (auto i = 0; i < 4; i++) {
                        auto length_expect = (1 << i);
                        if (padding_and_length_len < length_expect) {
                            return {};
                        }
                        auto padding_len = padding_and_length_len - length_expect;
                        auto actual_len = src.qvarintlen(length_value + padding_len);
                        if (actual_len == length_expect) {
                            length_value += padding_len;
                            payload_len += padding_len;
                            ok = true;
                            break;
                        }
                    }
                    if (!ok) {
                        return {};
                    }
                }
                packet.length = src.qvarint(length_value);
                packet.packet_number = src.packet_number(packet_number.value, packet_number.len);
                if (payload_len == payload.len) {
                    packet.payload = payload;
                }
                else {
                    if (!payload.enough(payload.len)) {
                        return {};
                    }
                    auto aqpayload = src.acquire(payload_len);
                    WPacket tmp{aqpayload};
                    tmp.append(payload.data, payload.len);
                    tmp.add(0, payload_len - payload.len);
                    packet.payload = aqpayload;
                }
                WPacket test;
                if (!packet.render(test, payload_tag_len)) {
                    return {};
                }
                if (reqlen && test.counter != reqlen) {
                    return {};
                }
                return packet;
            }

            constexpr crypto::CryptoPacketInfo make_cryptoinfo(WPacket& src, const InitialPacketPlain& packet, size_t payload_tag_len) {
                auto offset = src.offset;
                if (!packet.render(src, payload_tag_len)) {
                    return {};
                }
                if (src.overflow) {
                    return {};
                }
                auto len = src.offset - offset;
                src.offset = offset;
                auto target = src.acquire(len);
                if (!target.enough(len)) {
                    return {};
                }
                crypto::CryptoPacketInfo info;
                auto pn_len = packet.flags.packet_number_length();
                auto head_len = packet.as_partial().packet_len();
                auto payload_len = packet.packet_len() - head_len;
                info.head = target.resized(head_len);
                info.payload = target.forward(head_len).resized(payload_len);
                info.tag = target.forward(head_len + payload_len).resized(payload_tag_len);
                info.processed_payload = target.forward(head_len + pn_len).resized(payload_len - pn_len + payload_tag_len);
                return info;
            }

            constexpr crypto::CryptoPacketInfo make_cryptoinfo_from_cipher_packet(const auto& cipher, size_t tag_len) {
                crypto::CryptoPacketInfo info;
                size_t head_len = cipher.as_partial().packet_len();
                info.head = ByteLen{cipher.flags.value, head_len};
                if (cipher.payload.len < tag_len) {
                    return {};
                }
                info.payload = ByteLen{cipher.payload.data, cipher.payload.len - tag_len};
                info.tag = ByteLen{cipher.payload.data + cipher.payload.len - tag_len, tag_len};
                if (!info.composition().valid()) {
                    return {};
                }
                return info;
            }

        }  // namespace quic::packet
    }      // namespace dnet
}  // namespace utils

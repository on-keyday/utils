/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet - QUIC packet format
#pragma once
#include "bytelen.h"
#include <utility>

namespace utils {
    namespace dnet {
        namespace quic {

            struct Packet {
                PacketFlags flags;

                constexpr bool parse(ByteLen b) {
                    if (!b.enough(1)) {
                        return false;
                    }
                    flags = {b.data};
                    return true;
                }

                constexpr size_t packet_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    if (!flags.value) {
                        return false;
                    }
                    w.append(flags.value, 1);
                    return true;
                }
            };

            struct LongPacketBase : Packet {
                ByteLen version;
                byte* dstIDLen;
                ByteLen dstID;
                byte* srcIDLen;
                ByteLen srcID;

                constexpr bool parse(ByteLen& b) {
                    Packet::parse(b);
                    if (flags.is_short()) {
                        return false;
                    }
                    if (!b.fwdresize(version, 1, 4)) {
                        return false;
                    }
                    if (!b.fwdenough(4, 1)) {
                        return false;
                    }
                    dstIDLen = b.data;
                    if (!b.fwdresize(dstID, 1, *dstIDLen)) {
                        return false;
                    }
                    if (!b.fwdenough(*dstIDLen, 1)) {
                        return false;
                    }
                    srcIDLen = b.data;
                    if (!b.fwdresize(srcID, 1, *srcIDLen)) {
                        return false;
                    }
                    b = b.forward(*srcIDLen);
                    return true;
                }

                constexpr size_t packet_len() const {
                    return Packet::packet_len() +
                           4 +          // version
                           1 +          // dstIDLen
                           dstID.len +  // dstID
                           1 +          // srcIDLen
                           srcID.len;   // srcID
                }

                constexpr bool render(WPacket& w) const {
                    if (!Packet::render(w)) {
                        return false;
                    }
                    if (!version.enough(4) ||
                        !dstIDLen || !dstID.enough(*dstIDLen) ||
                        !srcIDLen || !srcID.enough(*srcIDLen)) {
                        return false;
                    }
                    w.append(version.data, 4);
                    w.append(dstIDLen, 1);
                    w.append(dstID.data, *dstIDLen);
                    w.append(srcIDLen, 1);
                    w.append(srcID.data, *srcIDLen);
                    return true;
                }
            };

            struct LongPacket : LongPacketBase {
                ByteLen payload;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    payload = b;
                    return true;
                }

                constexpr size_t packet_len() const {
                    return LongPacketBase::packet_len() + payload.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (!payload.valid()) {
                        return false;
                    }
                    w.append(payload.data, payload.len);
                    return true;
                }
            };

            struct VersionNegotiationPacket : LongPacketBase {
                ByteLen supported_versions;
                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type() != PacketType::VersionNegotiation) {
                        return false;
                    }
                    supported_versions = b;
                    return true;
                }

                constexpr size_t packet_len() const {
                    return LongPacketBase::packet_len() + supported_versions.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (flags.type() != PacketType::VersionNegotiation) {
                        return false;
                    }
                    if (!supported_versions.valid()) {
                        return false;
                    }
                    w.append(supported_versions.data, supported_versions.len);
                    return true;
                }
            };

            struct InitialPacketPartial : LongPacketBase {
                ByteLen token_length;
                ByteLen token;
                ByteLen length;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type() != PacketType::Initial) {
                        return false;
                    }
                    auto len = b.varintlen();
                    if (len == 0 || !b.enough(len)) {
                        return false;
                    }
                    token_length = b.resized(len);
                    auto toklen = token_length.varint();
                    if (!b.fwdresize(token, len, toklen)) {
                        return false;
                    }
                    b = b.forward(toklen);
                    len = b.varintlen();
                    if (len == 0 || !b.enough(len)) {
                        return false;
                    }
                    length = b.resized(len);
                    b = b.forward(len);
                    return true;
                }

                constexpr size_t packet_len() const {
                    return LongPacketBase::packet_len() +
                           token_length.len +
                           token.len +
                           length.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (flags.type() != PacketType::Initial) {
                        return false;
                    }
                    if (!token_length.is_varint_valid() ||
                        !token.enough(token_length.varint()) ||
                        !length.is_varint_valid()) {
                        return false;
                    }
                    w.append(token_length.data, token_length.varintlen());
                    w.append(token.data, token_length.varint());
                    w.append(length.data, length.varintlen());
                    return true;
                }
            };

            struct InitialPacket : InitialPacketPartial {
                ByteLen packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b) {
                    if (!InitialPacketPartial::parse(b)) {
                        return false;
                    }
                    auto len = flags.packet_number_length();
                    packet_number = b.resized(len);
                    b = b.forward(len);
                    payload = b;
                    return true;
                }

                constexpr size_t packet_len() const {
                    return InitialPacketPartial::packet_len() +
                           packet_number.len +
                           payload.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!InitialPacketPartial::render(w)) {
                        return false;
                    }
                    if (packet_number.len < flags.packet_number_length() ||
                        !payload.valid()) {
                        return false;
                    }
                    w.append(packet_number.data, flags.packet_number_length());
                    w.append(payload.data, payload.len);
                    return true;
                }
            };

            struct ZeroRTTPacketPartial : LongPacketBase {
                ByteLen length;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type() != PacketType::ZeroRTT) {
                        return false;
                    }
                    auto len = b.varintlen();
                    if (len == 0 || !b.enough(len)) {
                        return false;
                    }
                    length = b.resized(len);
                    b = b.forward(len);
                    return true;
                }

                constexpr size_t packet_len() const {
                    return LongPacketBase::packet_len() +
                           length.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (flags.type() != PacketType::ZeroRTT) {
                        return false;
                    }
                    if (!length.is_varint_valid()) {
                        return false;
                    }
                    w.append(length.data, length.varintlen());
                    return true;
                }
            };

            struct ZeroRTTPacket : ZeroRTTPacketPartial {
                ByteLen packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b) {
                    if (!ZeroRTTPacketPartial::parse(b)) {
                        return false;
                    }
                    auto len = flags.packet_number_length();
                    packet_number = b.resized(len);
                    b = b.forward(len);
                    payload = b;
                    return true;
                }

                constexpr size_t packet_len() const {
                    return ZeroRTTPacketPartial::packet_len() +
                           packet_number.len +
                           payload.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!ZeroRTTPacketPartial::render(w)) {
                        return false;
                    }
                    if (packet_number.len < flags.packet_number_length() ||
                        !payload.valid()) {
                        return false;
                    }
                    w.append(packet_number.data, flags.packet_number_length());
                    w.append(payload.data, payload.len);
                    return true;
                }
            };

            struct HandshakePacketPartial : LongPacketBase {
                ByteLen length;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type() != PacketType::HandShake) {
                        return false;
                    }
                    auto len = b.varintlen();
                    if (len == 0 || !b.enough(len)) {
                        return false;
                    }
                    length = b.resized(len);
                    b = b.forward(len);
                    return true;
                }

                constexpr size_t packet_len() const {
                    return LongPacketBase::packet_len() +
                           length.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (flags.type() != PacketType::HandShake) {
                        return false;
                    }
                    if (!length.is_varint_valid()) {
                        return false;
                    }
                    w.append(length.data, length.varintlen());
                    return true;
                }
            };

            struct HandshakePacket : HandshakePacketPartial {
                ByteLen packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b) {
                    if (!HandshakePacketPartial::parse(b)) {
                        return false;
                    }
                    auto len = flags.packet_number_length();
                    packet_number = b.resized(len);
                    b.forward(len);
                    payload = b;
                    return true;
                }

                constexpr size_t packet_len() const {
                    return HandshakePacketPartial::packet_len() +
                           length.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!HandshakePacketPartial::render(w)) {
                        return false;
                    }
                    if (packet_number.len < flags.packet_number_length() ||
                        !payload.valid()) {
                        return false;
                    }
                    w.append(packet_number.data, flags.packet_number_length());
                    w.append(payload.data, payload.len);
                    return true;
                }
            };

            struct RetryPacket : LongPacketBase {
                ByteLen retry_token;
                ByteLen integrity_tag;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type() != PacketType::Retry) {
                        return false;
                    }
                    if (!b.enough(16)) {
                        return false;
                    }
                    retry_token = b.resized(b.len - 16);
                    b = b.forward(b.len - 16);
                    integrity_tag = b.resized(16);
                    b = b.forward(16);
                    return true;
                }

                constexpr size_t packet_len() const {
                    return LongPacketBase::packet_len() +
                           retry_token.len +
                           integrity_tag.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (flags.type() != PacketType::Retry) {
                        return false;
                    }
                    if (!retry_token.valid() || !integrity_tag.enough(16)) {
                        return false;
                    }
                    w.append(retry_token.data, retry_token.len);
                    w.append(integrity_tag.data, 16);
                    return true;
                }
            };

            struct OneRTTPacketPartial : Packet {
                ByteLen dstID;

                constexpr bool parse(ByteLen& b, auto&& get_dstID_len) {
                    if (!Packet::parse(b)) {
                        return false;
                    }
                    if (flags.type() != PacketType::OneRTT) {
                        return false;
                    }
                    b.forward(1);
                    size_t len = get_dstID_len(std::as_const(b));
                    if (!b.enough(len)) {
                        return false;
                    }
                    dstID = b.resized(len);
                    b = b.forward(len);
                    return true;
                }

                constexpr size_t packet_len() {
                    return Packet::packet_len() + dstID.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Packet::render(w)) {
                        return false;
                    }
                    if (flags.type() != PacketType::OneRTT) {
                        return false;
                    }
                    if (!dstID.valid()) {
                        return false;
                    }
                    w.append(dstID.data, dstID.len);
                    return true;
                }
            };

            struct OneRTTPacket : OneRTTPacketPartial {
                ByteLen packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b, auto&& get_dstID_len) {
                    if (!OneRTTPacketPartial::parse(b, get_dstID_len)) {
                        return false;
                    }
                    auto len = flags.packet_number_length();
                    packet_number = b.resized(len);
                    b.forward(len);
                    payload = b;
                    return true;
                }

                constexpr size_t packet_len() {
                    return OneRTTPacketPartial::packet_len() +
                           packet_number.len +
                           payload.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!OneRTTPacketPartial::render(w)) {
                        return false;
                    }
                    if (packet_number.len < flags.packet_number_length() ||
                        !payload.valid()) {
                        return false;
                    }
                    w.append(packet_number.data, flags.packet_number_length());
                    w.append(payload.data, payload.len);
                    return true;
                }
            };

            struct StatelessReset : Packet {
                ByteLen unpredicable_bits;
                ByteLen stateless_reset_token;

                constexpr bool parse(ByteLen& b) {
                    if (!Packet::parse(b)) {
                        return true;
                    }
                    b.forward(1);
                    if (!b.enough(16)) {
                        return false;
                    }
                    unpredicable_bits = b.resized(b.len - 16);
                    b = b.forward(b.len - 16);
                    stateless_reset_token = b.resized(16);
                    b = b.forward(16);
                    return true;
                }

                constexpr size_t packet_len() {
                    return Packet::packet_len() +
                           unpredicable_bits.len +
                           stateless_reset_token.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Packet::render(w)) {
                        return false;
                    }
                    if (!unpredicable_bits.enough(4) || !stateless_reset_token.enough(16)) {
                        return false;
                    }
                    w.append(unpredicable_bits.data, unpredicable_bits.len);
                    w.append(stateless_reset_token.data, 16);
                    return true;
                }
            };

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

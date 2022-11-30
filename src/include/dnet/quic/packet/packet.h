/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet - QUIC packet format
#pragma once
#include "../../bytelen.h"
#include <utility>

namespace utils {
    namespace dnet {
        namespace quic::packet {

            // payload rendering consistancy level
            // not_render - not render payload. for payload creation with frame
            // payload_length - adopt payload.len parameter. not check consistency with Packet.length parameter
            // packet_length - adopt Packet.length. check payload.len consistency with Packet.length parameter
            // null_payload - optional used with bit or. replace payload with 0x00. not check payload.data but use only payload.len
            enum class PayloadLevel : std::uint16_t {
                not_render = 0x0000,
                payload_length = 0x0001,  // adopt payload.len parameter
                packet_length = 0x0002,   // adopt Packet.length parameter
                null_payload = 0x0100,    // replace payload with 0x00

                mask_length_ = 0x00ff,
                mask_bytes_ = 0xff00,
            };

            constexpr PayloadLevel operator|(const PayloadLevel& a, const PayloadLevel& b) {
                return PayloadLevel(std::uint16_t(a) | std::uint16_t(b));
            }

            constexpr PayloadLevel operator&(const PayloadLevel& a, const PayloadLevel& b) {
                return PayloadLevel(std::uint16_t(a) & std::uint16_t(b));
            }

            constexpr PayloadLevel length_opt(PayloadLevel level) {
                return (level & PayloadLevel::mask_length_);
            }

            constexpr PayloadLevel bytes_opt(PayloadLevel level) {
                return (level & PayloadLevel::mask_bytes_);
            }

            namespace internal {
                constexpr bool parse_packet_number_and_payload(ByteLen& b, auto& p, bool parse_payload) {
                    if (!p.packet_number.parse(b, p.flags.packet_number_length())) {
                        return false;
                    }
                    auto payload_len = p.length - p.flags.packet_number_length();
                    if (!b.enough(payload_len)) {
                        return false;
                    }
                    if (parse_payload) {
                        p.payload = b.resized(payload_len);
                        b = b.forward(payload_len);
                    }
                    return true;
                }

                constexpr bool parse_cipher_payload(ByteLen& b, auto& p, bool parse_payload) {
                    if (!b.enough(p.length)) {
                        return false;
                    }
                    if (parse_payload) {
                        p.payload = b.resized(p.length);
                        b = b.forward(p.length);
                    }
                    return true;
                }

                constexpr bool render_payload(WPacket& w, auto& p, PayloadLevel payload_level) {
                    if (bytes_opt(payload_level) == PayloadLevel::null_payload) {
                        w.add(0, p.payload.len);
                    }
                    else {
                        if (!p.payload.data && p.payload.len) {
                            return false;
                        }
                        w.append(p.payload.data, p.payload.len);
                    }
                    return true;
                }

                constexpr bool render_packet_number_and_payload(WPacket& w, auto& p, PayloadLevel payload_level) {
                    if (!p.packet_number.render(w, p.flags.packet_number_length())) {
                        return false;
                    }
                    auto level = length_opt(payload_level);
                    if (level != PayloadLevel::not_render) {
                        if (level == PayloadLevel::packet_length) {
                            if (p.payload.len != p.length - p.flags.packet_number_length()) {
                                return false;
                            }
                        }
                        return render_payload(w, p, payload_level);
                    }
                    return true;
                }

                constexpr bool render_cipher_payload(WPacket& w, auto& p, PayloadLevel payload_level) {
                    auto level = length_opt(payload_level);
                    if (level != PayloadLevel::not_render) {
                        if (level == PayloadLevel::packet_length) {
                            if (p.payload.len != p.length) {
                                return false;
                            }
                        }
                        return render_payload(w, p, payload_level);
                    }
                    return true;
                }

                constexpr size_t calc_plain_render_len(size_t partial, auto& p, PayloadLevel level) {
                    if (level == PayloadLevel::payload_length) {
                        return partial +
                               p.flags.packet_number_length() +
                               p.payload.len;
                    }
                    else if (level == PayloadLevel::packet_length) {
                        return partial + p.length;
                    }
                    return partial + p.flags.packet_number_length();
                }

                constexpr size_t calc_cipher_render_len(size_t partial, auto& p, PayloadLevel level) {
                    if (level == PayloadLevel::payload_length) {
                        return partial + p.payload.len;
                    }
                    else if (level == PayloadLevel::packet_length) {
                        return partial + p.length;
                    }
                    return partial;
                }

            }  // namespace internal
            struct Packet {
                PacketFlags flags;

                constexpr bool parse(ByteLen& b) {
                    if (!b.enough(1)) {
                        return false;
                    }
                    flags = {*b.data};
                    b = b.forward(1);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return 1;
                }

                constexpr bool render(WPacket& w) const {
                    w.add(flags.value, 1);
                    return true;
                }

                constexpr size_t render_len() const {
                    return 1;
                }
            };

            struct LongPacketBase : Packet {
                std::uint32_t version = 0;
                byte dstIDLen = 0;  // only parse
                ByteLen dstID;
                byte srcIDLen = 0;  // only parse
                ByteLen srcID;

                constexpr bool parse(ByteLen& b) {
                    if (!Packet::parse(b)) {
                        return false;
                    }
                    if (flags.is_short()) {
                        return false;
                    }
                    if (!b.as(version)) {
                        return false;
                    }
                    if (!b.fwdenough(4, 1)) {
                        return false;
                    }
                    dstIDLen = *b.data;
                    if (!b.fwdresize(dstID, 1, dstIDLen)) {
                        return false;
                    }
                    if (!b.fwdenough(dstIDLen, 1)) {
                        return false;
                    }
                    srcIDLen = *b.data;
                    if (!b.fwdresize(srcID, 1, srcIDLen)) {
                        return false;
                    }
                    b = b.forward(srcIDLen);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Packet::parse_len() +
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
                    w.as(version);
                    if (dstID.len > 0xff) {
                        return false;
                    }
                    w.add(byte(dstID.len), 1);
                    if (!dstID.data && dstID.len) {
                        return false;
                    }
                    w.append(dstID.data, dstID.len);
                    if (srcID.len > 0xff) {
                        return false;
                    }
                    w.add(byte(srcID.len), 1);
                    if (!srcID.data && srcID.len) {
                        return false;
                    }
                    w.append(srcID.data, srcID.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    return parse_len();
                }

                constexpr LongPacketBase& as_lpacket_base() {
                    return *this;
                }

                constexpr const LongPacketBase& as_lpacket_base() const {
                    return *this;
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

                constexpr size_t parse_len() const {
                    return LongPacketBase::parse_len() + payload.len;
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

                constexpr size_t render_len() const {
                    return LongPacketBase::render_len() + payload.len;
                }
            };

            struct VersionNegotiationPacket : LongPacketBase {
                ByteLen supported_versions;
                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type(version) != PacketType::VersionNegotiation) {
                        return false;
                    }
                    if (b.len % 4) {
                        return false;
                    }
                    supported_versions = b;
                    b = b.forward(b.len);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return LongPacketBase::parse_len() + supported_versions.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (flags.type(version) != PacketType::VersionNegotiation) {
                        return false;
                    }
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (!supported_versions.valid() ||
                        supported_versions.len % 4) {
                        return false;
                    }
                    w.append(supported_versions.data, supported_versions.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    return LongPacketBase::render_len() + supported_versions.len;
                }
            };

            struct InitialPacketPartial : LongPacketBase {
                QVarInt token_length;  // only parse
                ByteLen token;
                QVarInt length;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type(version) != PacketType::Initial) {
                        return false;
                    }
                    if (!token_length.parse(b)) {
                        return false;
                    }
                    if (!b.fwdresize(token, 0, token_length)) {
                        return false;
                    }
                    b = b.forward(token_length);
                    if (!length.parse(b)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    return LongPacketBase::parse_len() +
                           token_length.len +
                           token.len +
                           length.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (flags.type(version) != PacketType::Initial) {
                        return false;
                    }
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (!QVarInt{token.len}.render(w)) {
                        return false;
                    }
                    if (!token.data && token.len) {
                        return false;
                    }
                    w.append(token.data, token.len);
                    return length.render(w);
                }

                constexpr size_t render_len(bool inc_len = true) const {
                    return LongPacketBase::render_len() +
                           QVarInt{token.len}.minimum_len() +
                           token.len +
                           (inc_len ? length.minimum_len() : 0);
                }

                constexpr InitialPacketPartial& as_partial() {
                    return *this;
                }

                constexpr const InitialPacketPartial& as_partial() const {
                    return *this;
                }
            };

            struct InitialPacketPlain : InitialPacketPartial {
                QPacketNumber packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b, bool parse_payload = true) {
                    if (!InitialPacketPartial::parse(b)) {
                        return false;
                    }
                    return internal::parse_packet_number_and_payload(b, *this, parse_payload);
                }

                constexpr size_t parse_len() const {
                    return InitialPacketPartial::parse_len() +
                           packet_number.len +
                           payload.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel payload_level = PayloadLevel::payload_length) const {
                    if (!InitialPacketPartial::render(w)) {
                        return false;
                    }
                    return internal::render_packet_number_and_payload(w, *this, payload_level);
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    return internal::calc_plain_render_len(InitialPacketPartial::render_len(), *this, level);
                }
            };

            struct InitialPacketCipher : InitialPacketPartial {
                ByteLen payload;

                constexpr bool parse(ByteLen& b, bool parse_payload = true) {
                    if (!InitialPacketPartial::parse(b)) {
                        return false;
                    }
                    return internal::parse_cipher_payload(b, *this, parse_payload);
                }

                constexpr size_t parse_len() const {
                    return InitialPacketPartial::parse_len() + payload.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel payload_level = PayloadLevel::payload_length) const {
                    if (!InitialPacketPartial::render(w)) {
                        return false;
                    }
                    return internal::render_cipher_payload(w, *this, payload_level);
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    return internal::calc_cipher_render_len(InitialPacketPartial::render_len(), *this, level);
                }
            };

            struct ZeroRTTPacketPartial : LongPacketBase {
                QVarInt length;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type(version) != PacketType::ZeroRTT) {
                        return false;
                    }
                    return length.parse(b);
                }

                constexpr size_t parse_len() const {
                    return LongPacketBase::parse_len() +
                           length.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (flags.type(version) != PacketType::ZeroRTT) {
                        return false;
                    }
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    return length.render(w);
                }

                constexpr size_t render_len(bool inc_len = true) const {
                    return LongPacketBase::render_len() +
                           (inc_len ? length.minimum_len() : 0);
                }

                constexpr ZeroRTTPacketPartial& as_parital() {
                    return *this;
                }

                constexpr const ZeroRTTPacketPartial& as_partial() const {
                    return *this;
                }
            };

            struct ZeroRTTPacketPlain : ZeroRTTPacketPartial {
                QPacketNumber packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b, bool parse_payload = true) {
                    if (!ZeroRTTPacketPartial::parse(b)) {
                        return false;
                    }
                    return internal::parse_packet_number_and_payload(b, *this, parse_payload);
                }

                constexpr size_t parse_len() const {
                    return ZeroRTTPacketPartial::parse_len() +
                           packet_number.len +
                           payload.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel level = PayloadLevel::payload_length) const {
                    if (!ZeroRTTPacketPartial::render(w)) {
                        return false;
                    }
                    return internal::render_packet_number_and_payload(w, *this, level);
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    return internal::calc_plain_render_len(ZeroRTTPacketPartial::render_len(), *this, level);
                }
            };

            struct ZeroRTTPacketCipher : ZeroRTTPacketPartial {
                ByteLen payload;

                constexpr bool parse(ByteLen& b, bool parse_payload = true) {
                    if (!ZeroRTTPacketPartial::parse(b)) {
                        return false;
                    }
                    return internal::parse_cipher_payload(b, *this, parse_payload);
                }

                constexpr size_t parse_len() const {
                    return ZeroRTTPacketPartial::parse_len() + payload.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel level = PayloadLevel::payload_length) const {
                    if (!ZeroRTTPacketPartial::render(w)) {
                        return false;
                    }
                    return internal::render_cipher_payload(w, *this, level);
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    return internal::calc_cipher_render_len(ZeroRTTPacketPartial::render_len(), *this, level);
                }
            };

            struct HandshakePacketPartial : LongPacketBase {
                QVarInt length;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type(version) != PacketType::Handshake) {
                        return false;
                    }
                    return length.parse(b);
                }

                constexpr size_t parse_len() const {
                    return LongPacketBase::parse_len() +
                           length.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (flags.type(version) != PacketType::Handshake) {
                        return false;
                    }
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    return length.render(w);
                }

                constexpr size_t render_len(bool inc_len = true) const {
                    return LongPacketBase::render_len() +
                           (inc_len ? length.minimum_len() : 0);
                }

                HandshakePacketPartial& as_partial() {
                    return *this;
                }

                constexpr const HandshakePacketPartial& as_partial() const {
                    return *this;
                }
            };

            struct HandshakePacketPlain : HandshakePacketPartial {
                QPacketNumber packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b, bool parse_payload = true) {
                    if (!HandshakePacketPartial::parse(b)) {
                        return false;
                    }
                    return internal::parse_packet_number_and_payload(b, *this, parse_payload);
                }

                constexpr size_t packet_len() const {
                    return HandshakePacketPartial::parse_len() +
                           length.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel level = PayloadLevel::payload_length) const {
                    if (!HandshakePacketPartial::render(w)) {
                        return false;
                    }
                    return internal::render_packet_number_and_payload(w, *this, level);
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    return internal::calc_plain_render_len(HandshakePacketPartial::render_len(), *this, level);
                }
            };

            struct HandshakePacketCipher : HandshakePacketPartial {
                ByteLen payload;

                constexpr bool parse(ByteLen& b, bool parse_payload = true) {
                    if (!HandshakePacketPartial::parse(b)) {
                        return false;
                    }
                    return internal::parse_cipher_payload(b, *this, parse_payload);
                }

                constexpr size_t parse_len() const {
                    return HandshakePacketPartial::parse_len() + payload.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel level = PayloadLevel::payload_length) const {
                    if (!HandshakePacketPartial::render(w)) {
                        return false;
                    }
                    return internal::render_cipher_payload(w, *this, level);
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    return internal::calc_cipher_render_len(HandshakePacketPartial::render_len(), *this, level);
                }
            };

            struct RetryPacket : LongPacketBase {
                ByteLen retry_token;
                ByteLen integrity_tag;

                constexpr bool parse(ByteLen& b) {
                    if (!LongPacketBase::parse(b)) {
                        return false;
                    }
                    if (flags.type(version) != PacketType::Retry) {
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

                constexpr size_t parse_len() const {
                    return LongPacketBase::parse_len() +
                           retry_token.len +
                           integrity_tag.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (flags.type(version) != PacketType::Retry) {
                        return false;
                    }
                    if (!LongPacketBase::render(w)) {
                        return false;
                    }
                    if (!retry_token.valid() || !integrity_tag.data || integrity_tag.len != 16) {
                        return false;
                    }
                    w.append(retry_token.data, retry_token.len);
                    w.append(integrity_tag.data, 16);
                    return true;
                }

                constexpr size_t render_len() const {
                    return LongPacketBase::parse_len() + retry_token.len + 16;
                }
            };

            // rfc 9001 5.8. Retry Packet Integrity
            // this is not derived from Packet class
            struct RetryPseduoPacket {
                byte origDstIDlen = 0;  // only parse
                ByteLen origDstID;
                LongPacketBase long_packet;
                ByteLen retry_token;

                // this parse may be unused but implimented for API uniformity
                bool parse(ByteLen& b) {
                    if (!b.enough(1)) {
                        return false;
                    }
                    origDstIDlen = *b.data;
                    b = b.forward(1);
                    if (!b.enough(origDstIDlen)) {
                        return false;
                    }
                    origDstID = b.resized(origDstIDlen);
                    b = b.forward(origDstIDlen);
                    if (!long_packet.parse(b)) {
                        return false;
                    }
                    if (long_packet.flags.type(long_packet.version) != PacketType::Retry) {
                        return false;
                    }
                    retry_token = b;
                    b = b.forward(b.len);
                    return true;
                }

                size_t parse_len() const {
                    return 1 + origDstID.len + long_packet.parse_len() + retry_token.len;
                }

                bool render(WPacket& w) const {
                    if (origDstID.len > 0xff) {
                        return false;
                    }
                    if (!origDstID.data && origDstID.len) {
                        return false;
                    }
                    w.append(origDstID.data, origDstID.len);
                    if (!long_packet.render(w)) {
                        return false;
                    }
                    if (!retry_token.data && retry_token.len) {
                        return false;
                    }
                    w.append(retry_token.data, retry_token.len);
                    return true;
                }

                size_t render_len() const {
                    return 1 + origDstID.len + long_packet.render_len() + retry_token.len;
                }

                void from_retry_packet(ByteLen origdst, const RetryPacket& retry) {
                    origDstID = origdst;
                    long_packet = retry.as_lpacket_base();
                    retry_token = retry.retry_token;
                }
            };

            struct OneRTTPacketPartial : Packet {
                ByteLen dstID;

                // get_dstID_len is bool(const ByteLen& b,size_t* len)
                constexpr bool parse(ByteLen& b, auto&& get_dstID_len) {
                    if (!Packet::parse(b)) {
                        return false;
                    }
                    if (flags.type() != PacketType::OneRTT) {
                        return false;
                    }
                    size_t len = 0;
                    if (!get_dstID_len(std::as_const(b), &len)) {
                        return false;
                    }
                    if (!b.enough(len)) {
                        return false;
                    }
                    dstID = b.resized(len);
                    b = b.forward(len);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Packet::parse_len() + dstID.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (flags.type() != PacketType::OneRTT) {
                        return false;
                    }
                    if (!Packet::render(w)) {
                        return false;
                    }
                    if (!dstID.data && dstID.len) {
                        return false;
                    }
                    w.append(dstID.data, dstID.len);
                    return true;
                }

                constexpr size_t render_len() const {
                    return Packet::parse_len() + dstID.len;
                }

                OneRTTPacketPartial& as_partial() {
                    return *this;
                }

                constexpr const OneRTTPacketPartial& as_partial() const {
                    return *this;
                }
            };

            struct OneRTTPacketPlain : OneRTTPacketPartial {
                QPacketNumber packet_number;
                ByteLen payload;

                constexpr bool parse(ByteLen& b, auto&& get_dstID_len, bool parse_payload = true) {
                    if (!OneRTTPacketPartial::parse(b, get_dstID_len)) {
                        return false;
                    }
                    if (!packet_number.parse(b, flags.packet_number_length())) {
                        return false;
                    }
                    if (parse_payload) {
                        payload = b;
                        b = b.forward(b.len);
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    return OneRTTPacketPartial::parse_len() +
                           packet_number.len +
                           payload.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel level = PayloadLevel::payload_length) const {
                    if (!OneRTTPacketPartial::render(w)) {
                        return false;
                    }
                    if (!packet_number.render(w, flags.packet_number_length())) {
                        return false;
                    }
                    if (length_opt(level) != PayloadLevel::not_render) {
                        return internal::render_payload(w, *this, level);
                    }
                    return true;
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    if (length_opt(level) == PayloadLevel::not_render) {
                        return OneRTTPacketPartial::render_len() + flags.packet_number_length();
                    }
                    return OneRTTPacketPartial::render_len() + flags.packet_number_length() + payload.len;
                }
            };

            struct OneRTTPacketCipher : OneRTTPacketPartial {
                ByteLen payload;

                constexpr bool parse(ByteLen& b, auto&& get_dstID_len, bool parse_payload = true) {
                    if (!OneRTTPacketPartial::parse(b, get_dstID_len)) {
                        return false;
                    }
                    if (parse_payload) {
                        payload = b;
                        b = b.forward(b.len);
                    }
                    return true;
                }

                constexpr size_t parse_len() const {
                    return OneRTTPacketPartial::parse_len() + payload.len;
                }

                constexpr bool render(WPacket& w, PayloadLevel level = PayloadLevel::payload_length) const {
                    if (!OneRTTPacketPartial::render(w)) {
                        return false;
                    }
                    if (length_opt(level) != PayloadLevel::not_render) {
                        return internal::render_payload(w, *this, level);
                    }
                    return true;
                }

                constexpr size_t render_len(PayloadLevel level = PayloadLevel::payload_length) const {
                    if (length_opt(level) == PayloadLevel::not_render) {
                        return OneRTTPacketPartial::render_len();
                    }
                    return OneRTTPacketPartial::render_len() + payload.len;
                }
            };

            struct StatelessReset : Packet {
                ByteLen unpredicable_bits;
                ByteLen stateless_reset_token;

                constexpr bool parse(ByteLen& b) {
                    if (!Packet::parse(b)) {
                        return true;
                    }
                    if (!b.enough(16)) {
                        return false;
                    }
                    unpredicable_bits = b.resized(b.len - 16);
                    b = b.forward(b.len - 16);
                    stateless_reset_token = b.resized(16);
                    b = b.forward(16);
                    return true;
                }

                constexpr size_t parse_len() const {
                    return Packet::parse_len() +
                           unpredicable_bits.len +
                           stateless_reset_token.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!Packet::render(w)) {
                        return false;
                    }
                    if (unpredicable_bits.len < 4 || !stateless_reset_token.data || stateless_reset_token.len != 16) {
                        return false;
                    }
                    if (!unpredicable_bits.data) {
                        w.add(0, unpredicable_bits.len);
                    }
                    else {
                        w.append(unpredicable_bits.data, unpredicable_bits.len);
                    }
                    w.append(stateless_reset_token.data, 16);
                    return true;
                }

                constexpr size_t render_len() const {
                    return Packet::render_len() +
                           unpredicable_bits.len +
                           16;
                }
            };

        }  // namespace quic::packet
    }      // namespace dnet
}  // namespace utils

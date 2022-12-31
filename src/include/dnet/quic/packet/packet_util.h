/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_util - packet utility
#pragma once
#include "../../bytelen.h"
#include "packet.h"
#include "../crypto/crypto_packet.h"
#include "../version.h"

namespace utils {
    namespace dnet {
        namespace quic::packet {
            // cb is void(PacketType,Packet&,ByteLen src,bool err,bool valid_type) or
            // bool(PacketType,Packet&,ByteLen src,bool err,bool valid_type)
            constexpr bool parse_packet(ByteLen& b, auto&& cb, auto&& is_stateless_reset, auto&& get_dstID_len) {
                if (!b.enough(1)) {
                    return false;
                }
                auto copy = b;
                auto invoke_cb = [&](PacketType type, auto& packet, bool err, bool valid_type) {
                    auto delta = b.data - copy.data;
                    auto src = copy.resized(delta);
                    if constexpr (dnet::internal::has_logical_not<decltype(cb(type, packet, src, err, valid_type))>) {
                        if (!cb(type, packet, src, err, valid_type)) {
                            return false;
                        }
                    }
                    else {
                        cb(type, packet, src, err, valid_type);
                    }
                    return true;
                };
                auto flags = PacketFlags{*b.data};
                if (flags.is_long()) {
                    std::uint32_t version = 0;
                    if (!b.forward(1).as(version)) {
                        Packet packet;
                        packet.flags = flags;
                        invoke_cb(PacketType::Unknown, packet, true, false);
                        return false;
                    }
                    if (version == version_negotiation) {
                        VersionNegotiationPacket verneg;
                        if (!verneg.parse(b)) {
                            invoke_cb(PacketType::VersionNegotiation, verneg, true, true);
                            return false;
                        }
                        return invoke_cb(PacketType::VersionNegotiation, verneg, false, true);
                    }
                    auto type = flags.type(version);
#define PACKET(ptype, TYPE)                           \
    case ptype: {                                     \
        TYPE packet;                                  \
        if (!packet.parse(b)) {                       \
            invoke_cb(ptype, packet, true, true);     \
            return false;                             \
        }                                             \
        return invoke_cb(ptype, packet, false, true); \
    }
                    switch (type) {
                        PACKET(PacketType::Initial, InitialPacketCipher)
                        PACKET(PacketType::ZeroRTT, ZeroRTTPacketCipher)
                        PACKET(PacketType::Handshake, HandshakePacketCipher)
                        PACKET(PacketType::Retry, RetryPacket)
                        default:
                            break;
                    }
                    LongPacketBase base;
                    base.flags = flags;
                    base.version = version;
                    invoke_cb(PacketType::LongPacket, base, true, false);
                    return false;
#undef PACKET
                }
                {
                    auto copy2 = b;
                    StatelessReset reset;
                    if (reset.parse(copy2)) {
                        if (is_stateless_reset(reset)) {
                            b = copy2;
                            return invoke_cb(PacketType::StatelessReset, reset, false, true);
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

            constexpr bool parse_packets(ByteLen& b, auto&& cb, auto&& is_stateless_reset, auto&& get_dstID_len) {
                while (b.len) {
                    if (!parse_packet(b, cb, is_stateless_reset, get_dstID_len)) {
                        return false;
                    }
                }
                return true;
            }

            constexpr std::pair<size_t, size_t> calc_payload_length(size_t reqlen, size_t header_before_length, size_t packet_number_len, size_t payload_len, size_t payload_tag_len) {
                auto length_value = packet_number_len + payload_len + payload_tag_len;
                size_t padding_len = 0;
                if (reqlen) {
                    // check reqlen is larger than header_before_length and length_value
                    if (reqlen < header_before_length + length_value) {
                        return {};
                    }
                    // [header_before_length] // known   <--
                    // [length_len]           // unknown <-- in padding_and_length_len
                    // [packet_number.len]    // known   <-- in length_value
                    // [payload.len]          // known   <-- in length_value
                    // [padding_len]          // unknown <-- in padding_and_length_len
                    // [payload_tag_len]      // known   <-- in length_value
                    auto padding_and_length_len = reqlen - header_before_length - length_value;
                    bool ok = false;
                    for (auto i = 0; i < 4; i++) {
                        // length expect is 1,2,4,or 8
                        auto length_expect = (1 << i);
                        if (padding_and_length_len < length_expect) {
                            return {0, 0};
                        }
                        padding_len = padding_and_length_len - length_expect;
                        if (QVarInt{length_value + padding_len}.minimum_len() == length_expect) {
                            length_value += padding_len;
                            return {length_value, padding_len};
                        }
                    }
                    return {0, 0};
                }
                return {length_value, 0};
            }

            namespace internal {
                template <class PPacket>
                constexpr std::pair<PPacket, size_t> test_plain(PPacket& plain, size_t padding_len, size_t payload_tag_len, size_t reqlen) {
                    WPacket test;
                    if (!plain.render(test, PayloadLevel::payload_length | PayloadLevel::null_payload)) {
                        return {};
                    }
                    test.add(0, padding_len);
                    test.add(0, payload_tag_len);
                    if (reqlen && test.counter != reqlen) {
                        return {};
                    }
                    return {plain, padding_len};
                }

                template <PacketType type, class PPacket>
                constexpr std::pair<PPacket, size_t> make_handshake_zerortt_plain(
                    PPacket& plain, QPacketNumber packet_number, std::uint32_t version,
                    ByteLen dstID, ByteLen srcID, ByteLen payload,
                    size_t payload_tag_len, size_t reqlen) {
                    plain.flags = make_packet_flags(version, type, packet_number.len);
                    if (plain.flags.value == 0) {
                        return {};
                    }
                    plain.version = version;
                    plain.dstID = dstID;
                    plain.srcID = srcID;
                    auto [length_value, padding_len] = calc_payload_length(
                        reqlen,
                        plain.as_partial().render_len(false),
                        packet_number.len,
                        payload.len,
                        payload_tag_len);
                    if (length_value == 0) {
                        return {};
                    }
                    plain.length = length_value;
                    plain.packet_number = packet_number;
                    plain.payload = payload;
                    return test_plain(plain, padding_len, payload_tag_len, reqlen);
                }

                constexpr auto should_render() {
                    return [](WPacket& src) {
                        return false;
                    };
                }
            }  // namespace internal

            constexpr std::pair<InitialPacketPlain, size_t> make_initial_plain(
                QPacketNumber packet_number, std::uint32_t version,
                ByteLen dstConnID, ByteLen srcConnID,
                ByteLen token, ByteLen payload, size_t payload_tag_len, size_t reqlen) {
                InitialPacketPlain plain;
                plain.flags = make_packet_flags(version, PacketType::Initial, packet_number.len);
                if (plain.flags.value == 0) {
                    return {};
                }
                plain.version = version;
                plain.dstID = dstConnID;
                plain.srcID = srcConnID;
                plain.token = token;
                auto [length_value, padding_len] = calc_payload_length(
                    reqlen,
                    plain.as_partial().render_len(false),
                    packet_number.len,
                    payload.len,
                    payload_tag_len);
                if (length_value == 0) {
                    return {};
                }
                plain.length = length_value;
                plain.packet_number = packet_number;
                plain.payload = payload;
                return internal::test_plain(plain, padding_len, payload_tag_len, reqlen);
            }

            constexpr std::pair<HandshakePacketPlain, size_t> make_handshake_plain(
                QPacketNumber packet_number, std::uint32_t version,
                ByteLen dstID, ByteLen srcID, ByteLen payload,
                size_t payload_tag_len, size_t reqlen) {
                HandshakePacketPlain plain;
                return internal::make_handshake_zerortt_plain<PacketType::Handshake>(plain, packet_number, version, dstID, srcID, payload, payload_tag_len, reqlen);
            }

            constexpr std::pair<ZeroRTTPacketPlain, size_t> make_zerortt_plain(
                QPacketNumber packet_number, std::uint32_t version,
                ByteLen dstID, ByteLen srcID, ByteLen payload,
                size_t payload_tag_len, size_t reqlen) {
                ZeroRTTPacketPlain plain;
                return internal::make_handshake_zerortt_plain<PacketType::ZeroRTT>(plain, packet_number, version, dstID, srcID, payload, payload_tag_len, reqlen);
            }

            constexpr RetryPacket make_retry(std::uint32_t version, ByteLen dstID, ByteLen srcID, ByteLen retry_token, ByteLen integrity_tag) {
                RetryPacket retry;
                retry.flags = make_packet_flags(version, PacketType::Retry, 0);
                if (retry.flags.value == 0) {
                    return {};
                }
                retry.dstID = dstID;
                retry.srcID = srcID;
                retry.retry_token = retry_token;
                retry.integrity_tag = integrity_tag;
                return retry;
            }

            constexpr std::pair<OneRTTPacketPlain, size_t> make_onertt_plain(
                QPacketNumber packet_number, std::uint32_t version, bool spin, bool key_phase,
                ByteLen dstID, ByteLen payload, size_t payload_tag_len, size_t reqlen) {
                OneRTTPacketPlain plain;
                plain.flags = make_packet_flags(version, PacketType::OneRTT, packet_number.len, spin, key_phase);
                if (plain.flags.value == 0) {
                    return {};
                }
                plain.dstID = dstID;
                plain.packet_number = packet_number;
                plain.payload = payload;
                size_t padding_len = 0;
                if (reqlen) {
                    auto known_usage = plain.render_len() + payload_tag_len;
                    if (reqlen < known_usage) {
                        return {};
                    }
                    padding_len = reqlen - known_usage;
                }
                return internal::test_plain(plain, padding_len, payload_tag_len, reqlen);
            }

            template <class PPacket, class Fn = decltype(internal::should_render())>
            constexpr crypto::CryptoPacketInfo make_cryptoinfo_from_plain_packet(WPacket& src, const PPacket& packet, size_t payload_tag_len, PayloadLevel level = PayloadLevel::packet_length, Fn&& render_payload = internal::should_render()) {
                auto origin = src.offset;
                if (!packet.render(src, level)) {
                    return {};
                }
                if (length_opt(level) == PayloadLevel::not_render) {
                    if (!render_payload(src)) {
                        return {};
                    }
                }
                else if (length_opt(level) == PayloadLevel::packet_length) {
                    src.add(0, payload_tag_len);
                }
                if (src.overflow) {
                    return {};
                }
                auto packet_len = src.offset - origin;
                src.offset = origin;
                auto target = src.acquire(packet_len);
                PPacket parsed;
                auto copy = target;
                if constexpr (std::is_same_v<PPacket, OneRTTPacketPlain>) {
                    if (!parsed.parse(copy, [&](auto, size_t* l) {
                            *l = packet.dstID.len;
                            return true;
                        }) ||
                        copy.len) {
                        return {};
                    }
                }
                else {
                    if (!parsed.parse(copy) || copy.len) {
                        return {};
                    }
                }
                crypto::CryptoPacketInfo info;
                auto pn_len = parsed.flags.packet_number_length();
                auto head_len = parsed.as_partial().parse_len();
                auto payload_len = parsed.parse_len() - head_len - payload_tag_len;
                info.head = target.resized(head_len);
                info.payload = target.forward(head_len).resized(payload_len);
                info.tag = target.forward(head_len + payload_len).resized(payload_tag_len);
                info.processed_payload = target.forward(head_len + pn_len).resized(payload_len - pn_len + payload_tag_len);
                return info;
            }

            constexpr crypto::CryptoPacketInfo make_cryptoinfo_from_cipher_packet(const auto& cipher, ByteLen src, size_t tag_len) {
                auto parse_len = cipher.parse_len();
                if (!src.data || parse_len != src.len) {
                    return {};
                }
                if (cipher.flags.value != src.data[0]) {
                    return {};
                }
                crypto::CryptoPacketInfo info;
                size_t head_len = cipher.as_partial().parse_len();
                info.head = ByteLen{src.data, head_len};
                if (cipher.payload.len < tag_len) {
                    return {};
                }
                info.payload = ByteLen{src.data + head_len, cipher.payload.len - tag_len};
                if (!cipher.payload.data || info.payload.data[0] != cipher.payload.data[0]) {
                    return {};
                }
                info.tag = ByteLen{src.data + head_len + cipher.payload.len - tag_len, tag_len};
                if (info.composition().len != parse_len) {
                    return {};
                }
                return info;
            }

            constexpr auto packet_type_to_Packet(PacketType type, auto&& cb) {
                switch (type) {
#define MAP(t, Tplain, Tcipher) \
    case t:                     \
        return cb(Tplain{}, Tcipher{});
                    MAP(PacketType::Initial, InitialPacketPlain, InitialPacketCipher)
                    MAP(PacketType::ZeroRTT, ZeroRTTPacketPlain, ZeroRTTPacketCipher)
                    MAP(PacketType::Handshake, HandshakePacketPlain, HandshakePacketCipher)
                    MAP(PacketType::Retry, RetryPacket, RetryPacket)
                    MAP(PacketType::LongPacket, LongPacket, LongPacket)
                    MAP(PacketType::OneRTT, OneRTTPacketPlain, OneRTTPacketCipher)
                    MAP(PacketType::VersionNegotiation, VersionNegotiationPacket, VersionNegotiationPacket)
                    MAP(PacketType::StatelessReset, StatelessReset, StatelessReset)
#undef MAP
                    default:
                        return cb(Packet{}, Packet{});
                }
            }

            constexpr size_t guess_packet_with_payload_len(PacketType typ, size_t dstIDlen, size_t srcIDlen, size_t tokenlen, QPacketNumber pn, size_t payload_len) {
                return packet::packet_type_to_Packet(typ, [&](auto a, auto b) -> size_t {
                    if constexpr (!std::is_same_v<decltype(a), decltype(b)>) {
                        a.flags.value |= byte(pn.len - 1);
                        if constexpr (std::is_same_v<decltype(a), packet::OneRTTPacketPlain>) {
                            a.dstID = {nullptr, dstIDlen};
                            a.packet_number = pn;
                        }
                        else {
                            a.dstID = {nullptr, dstIDlen};
                            a.srcID = {nullptr, srcIDlen};
                            if constexpr (std::is_same_v<decltype(a), packet::InitialPacketPlain>) {
                                a.token = {nullptr, tokenlen};
                            }
                            a.length = payload_len + pn.len;
                            a.packet_number = pn;
                        }
                        a.payload = {nullptr, payload_len};
                        return a.render_len();
                    }
                    return 0;
                });
            }

            // returns (initial,handshake_zerortt,onertt)
            constexpr std::tuple<size_t, size_t, size_t> guess_header_len(size_t dstID, size_t srcID, size_t token, QPacketNumber pnini, QPacketNumber pnhs, QPacketNumber pnapp) {
                return {
                    guess_packet_with_payload_len(PacketType::Initial, dstID, srcID, token, pnini, 0),
                    guess_packet_with_payload_len(PacketType::Handshake, dstID, srcID, token, pnhs, 0),
                    guess_packet_with_payload_len(PacketType::OneRTT, dstID, srcID, token, pnapp, 0),
                };
            }

            struct GuessTuple {
                // frame count
                size_t count;
                // full packet length (header and payload)
                size_t packet_len;
                // pure payload length (exclude packet_number,cipher tag)
                size_t payload_len;
                // limit reached
                bool limit_reached;
                // length field value (packet_number,payload,cipher tag)
                QVarInt length_field;
            };

            // void cb(size_t index,size_t& payload_len)
            // returns (count,packet_len,payload_len,limit_reached,length)
            constexpr GuessTuple guess_packet_and_payload_len(size_t limit, PacketType type, size_t dstIDlen, size_t srcIDlen, size_t tokenlen, QPacketNumber pn, size_t payload_tag_len, auto&& cb) {
                size_t i = 0;
                size_t payload_len = 0;
                size_t prev_payload_len = 0;
                size_t prev_packet_len = 0;
                for (;;) {
                    QVarInt qv{pn.len + prev_payload_len + payload_tag_len};
                    qv.len = qv.minimum_len();
                    cb(i, payload_len);
                    if (payload_len <= prev_payload_len) {
                        return {
                            i,
                            prev_packet_len,
                            prev_payload_len,
                            false,
                            qv,
                        };
                    }
                    auto packet_len = guess_packet_with_payload_len(type, dstIDlen, srcIDlen, tokenlen, pn, payload_len + payload_tag_len);
                    if (packet_len > limit) {
                        return {i, prev_packet_len, prev_payload_len, true, qv};
                    }
                    prev_packet_len = packet_len;
                    prev_payload_len = payload_len;
                    i++;
                }
            }

        }  // namespace quic::packet
    }      // namespace dnet
}  // namespace utils

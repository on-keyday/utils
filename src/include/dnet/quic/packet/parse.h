/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// parse - packet parse
#pragma once
#include "wire.h"
#include "../version.h"

namespace utils {
    namespace dnet::quic::packet {
        // cb is void(PacketType,Packet&,view::rvec src,bool err,bool valid_type) or
        // bool(PacketType,Packet&,view::rvec src,bool err,bool valid_type)
        // Vec is for VersionNegotiationPacket
        template <template <class...> class Vec>
        constexpr bool parse_packet(io::reader& r, size_t tag_len, auto&& cb, auto&& is_stateless_reset, auto&& get_dstID_len) {
            if (r.empty()) {
                return false;
            }
            size_t offset = r.offset();
            auto invoke_cb = [&](PacketType type, auto& packet, bool err, bool valid_type) {
                auto src = r.already_read().substr(offset);
                if constexpr (std::is_convertible_v<decltype(cb(type, packet, src, err, valid_type)), bool>) {
                    if (!cb(type, packet, src, err, valid_type)) {
                        return false;
                    }
                }
                else {
                    cb(type, packet, src, err, valid_type);
                }
                return true;
            };
            auto flags = PacketFlags{r.top()};
            if (flags.is_long()) {
                std::uint32_t version = 0;
                auto peek = r.peeker();
                peek.offset(1);
                if (!io::read_num(peek, version, true)) {
                    Packet packet;
                    packet.flags = flags;
                    invoke_cb(PacketType::Unknown, packet, true, false);
                    return false;
                }
                if (version == version_negotiation) {
                    VersionNegotiationPacket<Vec> verneg;
                    if (!verneg.parse(r)) {
                        invoke_cb(PacketType::VersionNegotiation, verneg, true, true);
                        return false;
                    }
                    return invoke_cb(PacketType::VersionNegotiation, verneg, false, true);
                }
                auto type = flags.type(version);
#define CIPHER_PACKET(ptype, TYPE)                    \
    case ptype: {                                     \
        TYPE packet;                                  \
        if (!packet.parse(r, tag_len)) {              \
            invoke_cb(ptype, packet, true, true);     \
            return false;                             \
        }                                             \
        return invoke_cb(ptype, packet, false, true); \
    }
                switch (type) {
                    CIPHER_PACKET(PacketType::Initial, InitialPacketCipher)
                    CIPHER_PACKET(PacketType::ZeroRTT, ZeroRTTPacketCipher)
                    CIPHER_PACKET(PacketType::Handshake, HandshakePacketCipher)
                    case PacketType::Retry: {
                        RetryPacket retry;
                        if (!retry.parse(r)) {
                            invoke_cb(PacketType::Retry, retry, true, true);
                            return false;
                        }
                        return invoke_cb(PacketType::Retry, retry, false, true);
                    }
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
                auto peek = r.peeker();
                StatelessReset reset;
                if (reset.parse(peek)) {
                    if (is_stateless_reset(reset)) {
                        r = peek;
                        return invoke_cb(PacketType::StatelessReset, reset, false, true);
                    }
                }
            }
            OneRTTPacketCipher packet;
            if (!packet.parse(r, tag_len, get_dstID_len)) {
                invoke_cb(PacketType::OneRTT, packet, true, true);
                return false;
            }
            return invoke_cb(PacketType::OneRTT, packet, false, true);
        }

        template <template <class...> class Vec>
        constexpr bool parse_packets(io::reader& r, size_t auth_tag_len, auto&& cb, auto&& is_stateless_reset, auto&& get_dstID_len) {
            while (!r.empty()) {
                if (!parse_packet<Vec>(r, auth_tag_len, cb, is_stateless_reset, get_dstID_len)) {
                    return false;
                }
            }
            return true;
        }

        // this version function call cb with view::wvec src
        template <template <class...> class Vec>
        constexpr bool parse_packets(view::wvec src, size_t auth_tag_len, auto&& cb, auto&& is_stateless_reset, auto&& get_dstID_len) {
            io::reader r{src};
            auto wrap_cb = [&](PacketType type, auto&& packet, view::rvec rsrc, bool err, bool valid_type) {
                auto writable = const_cast<byte*>(rsrc.data());
                return cb(type, packet, view::wvec(writable, rsrc.size()), err, valid_type);
            };
            return parse_packets<Vec>(r, auth_tag_len, wrap_cb, is_stateless_reset, get_dstID_len);
        }

        // crypto_cb is bool(PacketType,Packet,CryptoPacket)
        // plain_cb is bool(PacketType,Packet,view::wvec src)
        // err_cb is bool(PacketType,Packet,view::wvec src,bool err,bool valid_type)
        constexpr auto with_crypto_parse(auto&& crypto_cb, auto&& plain_cb, auto&& err_cb) {
            return [=](PacketType type, auto&& packet, view::wvec src, bool err, bool valid_type) -> bool {
                using P = std::decay_t<decltype(packet)>;
                if (!valid_type || err) {
                    return err_cb(type, packet, src, err, valid_type);
                }
                if constexpr (std::is_same_v<P, InitialPacketCipher> ||
                              std::is_same_v<P, HandshakePacketCipher> ||
                              std::is_same_v<P, ZeroRTTPacketCipher> ||
                              std::is_same_v<P, OneRTTPacketCipher>) {
                    CryptoPacket crp;
                    crp.src = src;
                    crp.head_len = packet.head_len();
                    return crypto_cb(type, packet, crp);
                }
                return plain_cb(type, packet, src);
            };
        }

        template <class P>
        constexpr auto cipher_to_plain() {
            using T = std::decay_t<P>;
            if constexpr (std::is_same_v<T, OneRTTPacketCipher>) {
                return OneRTTPacketPlain{};
            }
            else if constexpr (std::is_same_v<T, InitialPacketCipher>) {
                return InitialPacketPlain{};
            }
            else if constexpr (std::is_same_v<T, HandshakePacketCipher>) {
                return HandshakePacketPlain{};
            }
            else if constexpr (std::is_same_v<T, ZeroRTTPacketCipher>) {
                return ZeroRTTPacketPlain{};
            }
            else {
                static_assert(std::is_same_v<T, OneRTTPacketCipher>,
                              "T should be cipher type");
            }
        }

        template <class T>
        constexpr auto convert_cipher_to_plain(CryptoPacket parsed, T&& in, size_t tag_len) {
            auto plain = cipher_to_plain<T>();
            io::reader r{parsed.src};
            bool ok = false;
            if constexpr (std::is_same_v<decltype(plain), OneRTTPacketPlain>) {
                ok = plain.parse(r, tag_len, [&](auto&&, size_t* len) {
                    *len = in.dstID.size();
                    return true;
                });
            }
            else {
                ok = plain.parse(r, tag_len);
            }
            return std::pair<decltype(plain), bool>{plain, ok};
        }

        constexpr PacketSummary summarize(auto&& packet, packetnum::Value pn = packetnum::infinity) noexcept {
            using T = std::decay_t<decltype(packet)>;
            PacketSummary summary;
            if constexpr (is_CryptoLongPacketPlain_v<T>) {
                summary.type = T::get_packet_type();
                summary.version = packet.version;
                summary.dstID = packet.dstID;
                summary.srcID = packet.srcID;
                if constexpr (std::is_same_v<T, InitialPacketPlain>) {
                    summary.token = packet.token;
                }
                summary.packet_number = pn;
            }
            else if constexpr (std::is_same_v<T, OneRTTPacketPlain>) {
                summary.type = PacketType::OneRTT;
                summary.dstID = packet.dstID;
                summary.packet_number = pn;
                summary.spin = packet.flags.spin_bit();
                summary.key = packet.flags.key_phase();
            }
            else {
                static_assert(std::is_same_v<T, OneRTTPacketPlain>, "in the future, summarize retry,version_negotioan,stateless_reset");
            }
            return summary;
        }

    }  // namespace dnet::quic::packet
}  // namespace utils

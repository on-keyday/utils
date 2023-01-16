/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// creation - packet creation
#pragma once
#include "wire.h"

namespace utils {
    namespace dnet::quic::packet {

        constexpr std::pair<CryptoPacket, bool> create_packet(io::writer& w, PacketSummary summary,
                                                              std::uint64_t largest_acked, size_t auth_tag_len, bool use_full,
                                                              auto&& render_payload) {
            auto tmp = packetnum::encode(summary.packet_number, largest_acked);
            if (!tmp.second) {
                return {{}, false};
            }
            auto wire = tmp.first;
            std::uint64_t payload_len = 0;
            auto offset = w.offset();
            auto get_crypto_packet = [&]() -> std::pair<CryptoPacket, bool> {
                CryptoPacket cpacket;
                cpacket.src = w.written().substr(offset);
                cpacket.head_len = cpacket.src.size() - payload_len - auth_tag_len - wire.len;
                cpacket.packet_number = summary.packet_number;
                return {cpacket, true};
            };
            auto with_padding = [&](io::writer& w) {
                auto poffset = w.offset();
                if (!render_payload(w)) {
                    return false;
                }
                if (use_full && !w.full()) {
                    w.write(0, w.remain().size());
                }
                payload_len = w.offset() - poffset;
                return true;
            };
            auto render_long = [&](auto& plain) {
                if (!plain.render_in_place(w, wire, with_padding, auth_tag_len, 0)) {
                    return std::pair<CryptoPacket, bool>{{}, false};
                }
                return get_crypto_packet();
            };
            if (summary.type == PacketType::Initial) {
                InitialPacketPlain plain;
                plain.version = summary.version;
                plain.dstID = summary.dstID;
                plain.srcID = summary.srcID;
                plain.token = summary.token;
                return render_long(plain);
            }
            else if (summary.type == PacketType::Handshake) {
                HandshakePacketPlain plain;
                plain.version = summary.version;
                plain.dstID = summary.dstID;
                plain.srcID = summary.srcID;
                return render_long(plain);
            }
            else if (summary.type == PacketType::ZeroRTT) {
                ZeroRTTPacketPlain plain;
                plain.version = summary.version;
                plain.dstID = summary.dstID;
                plain.srcID = summary.srcID;
                return render_long(plain);
            }
            else if (summary.type == PacketType::OneRTT) {
                OneRTTPacketPlain plain;
                plain.dstID = summary.dstID;
                if (!plain.render_in_place(w, summary.version, wire, summary.key, with_padding, auth_tag_len, 0, summary.spin)) {
                    return {{}, false};
                }
                return get_crypto_packet();
            }
            return {{}, false};
        }

        namespace test {
            constexpr bool check_packet_creation() {
                byte data[1200]{};
                io::writer w{data};
                byte id[20]{};
                PacketSummary summary;
                summary.type = PacketType::Initial;
                summary.version = 1;
                summary.dstID = id;
                summary.srcID = id;
                auto [crypto, ok] = create_packet(w, summary, packetnum::infinity, 16, true, [&](io::writer& w) {
                    return w.write(1, 250);
                });
                if (!ok) {
                    return false;
                }
                InitialPacketPlain plain;
                io::reader r{w.written()};
                if (!plain.parse(r, 16)) {
                    return false;
                }
                return plain.head_len(true) == crypto.head_len &&
                       plain.payload[0] == 1 &&
                       plain.payload[249] == 1 &&
                       plain.payload[250] == 0 &&
                       plain.auth_tag[0] == 0 &&
                       r.empty() &&
                       plain.len(true) == 1200;
            }

            static_assert(check_packet_creation());
        }  // namespace test
    }      // namespace dnet::quic::packet
}  // namespace utils

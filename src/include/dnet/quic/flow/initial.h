/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// initial - connection initialization helper of QUIC
#pragma once
#include "../packet.h"
#include "../frame.h"

namespace utils {
    namespace dnet {
        namespace quic::flow {
            constexpr InitialPacket make_first_flight(
                WPacket& src, std::uint32_t packet_number, byte pn_len, ByteLen crypto_data,
                ByteLen dstID, ByteLen srcID, ByteLen token) {
                InitialPacket packet;
                packet.flags = src.packet_flags(PacketType::Initial, pn_len);
                if (!packet.flags.value) {
                    return {};
                }
                packet.version = src.as<std::uint32_t>(1);
                packet.dstIDLen = src.as_byte(dstID.len);
                packet.dstID = dstID;
                packet.srcIDLen = src.as_byte(srcID.len);
                packet.srcID = srcID;
                packet.token_length = src.varint(token.len);
                packet.token = token;
                CryptoFrame crframe;
                crframe.type = src.frame_type(FrameType::CRYPTO);
                crframe.offset = src.varint(0);
                crframe.length = src.varint(crypto_data.len);
                crframe.crypto_data = crypto_data;
                auto before_length_field_len = packet.packet_len();
                auto crypto_frame_len = crframe.frame_len();
                auto actual_data_len = (before_length_field_len + 2 + packet.flags.packet_number_length() + crypto_frame_len);
                auto padding_len = 1200 - (actual_data_len + 16);
                WPacket tmp{src.acquire(crypto_frame_len + padding_len)};
                if (!crframe.render(tmp)) {
                    return {};
                }
                tmp.add(0, padding_len);
                packet.length = src.varint(packet.flags.packet_number_length() + crypto_frame_len + padding_len + 16);
                packet.packet_number = src.packet_number(packet_number, packet.flags.packet_number_length());
                packet.payload = tmp.b;
                return packet;
            }

            constexpr PacketInfo makePacketInfo(WPacket& dstw, InitialPacket& packet, bool enc) {
                PacketInfo info;
                dstw.offset = 0;
                if (!packet.render(dstw)) {
                    return {};
                }
                if (dstw.offset != packet.packet_len()) {
                    return {};
                }
                auto before_packet_number = static_cast<InitialPacketPartial&>(packet).packet_len();
                auto whole_len = packet.packet_len();
                info.head = dstw.b.resized(before_packet_number);
                info.payload = dstw.b.forward(before_packet_number)
                                   .resized(packet.flags.packet_number_length() + packet.payload.len);
                info.dstID = packet.dstID;
                auto pn_len = packet.flags.packet_number_length();
                info.processed_payload = dstw.b.forward(before_packet_number + pn_len)
                                             .resized(packet.payload.len + (enc ? 16 : 0));
                if (enc) {
                    dstw.offset += 16;
                }
                return info;
            }

            // parseInitialPartial parses initial packet from remote undecyrpted.
            // if clientDstID is valid, it will set to packetinfo.dstID
            // otherwise packet.dstID will set to packetinfo.dstID
            constexpr PacketInfo parseInitialPartial(ByteLen b, ByteLen clientDstID, auto&& version_validate) {
                InitialPacketPartial partial;
                auto copy = b;
                if (!partial.parse(copy)) {
                    return {};
                }
                if (!version_validate(partial.version.as<std::uint32_t>())) {
                    return {};
                }
                dnet::quic::PacketInfo info;
                info.head = b.resized(partial.packet_len());
                auto plength = partial.length.varint();
                info.payload = b.forward(partial.packet_len()).resized(plength);
                if (clientDstID.valid()) {
                    info.dstID = clientDstID;
                }
                else {
                    info.dstID = partial.dstID;
                }
                return info;
            }
        }  // namespace quic::flow
    }      // namespace dnet
}  // namespace utils

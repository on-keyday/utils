/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto - crypto packet data
#pragma once
#include "../../../view/iovec.h"
#include "../packet_number.h"

namespace utils {
    namespace dnet::quic::packet {

        // CryptoPacketParsed.auth_tag also can be included in sample
        // see https://tex2e.github.io/rfc-translater/html/rfc9001.html#A-5--ChaCha20-Poly1305-Short-Header-Packet
        struct CryptoPacketParsed {
            view::wvec head;    // exclude packet_number
            view::rvec sample;  // read only
            view::wvec protected_payload;
            view::wvec auth_tag;
        };

        struct CryptoPacketPnKnown {
            view::rvec head;  // include packet_number (read only)
            view::wvec protected_payload;
            view::wvec auth_tag;
        };

        struct CryptoPacket {
            view::wvec src;
            size_t head_len;  // exclude packet_number field length
            packetnum::Value packet_number = packetnum::infinity;
            constexpr static int skip_part = 4;

            constexpr std::pair<CryptoPacketParsed, bool> parse(size_t sample_len, size_t tag_len) const noexcept {
                CryptoPacketParsed parsed;

                parsed.head = src.substr(0, head_len);
                size_t payload_len = src.size() - head_len - tag_len;
                parsed.protected_payload = src.substr(head_len, payload_len);
                parsed.auth_tag = src.substr(head_len + payload_len, tag_len);
                parsed.sample = src.substr(head_len + skip_part, sample_len);
                if (parsed.head.size() != head_len ||
                    parsed.protected_payload.size() != payload_len ||
                    parsed.auth_tag.size() != tag_len ||
                    parsed.sample.size() != sample_len) {
                    return {{}, false};
                }
                if (src.size() != head_len + payload_len + tag_len) {
                    return {{}, false};
                }
                return {parsed, true};
            }

            constexpr std::pair<CryptoPacketPnKnown, bool> parse_pnknown(byte pnlen, size_t tag_len) const noexcept {
                if (!packetnum::is_wire_len(pnlen)) {
                    return {{}, false};
                }
                CryptoPacketPnKnown parsed;
                parsed.head = src.substr(0, head_len + pnlen);
                size_t payload_len = src.size() - (head_len + pnlen + tag_len);
                parsed.protected_payload = src.substr(head_len + pnlen, payload_len);
                parsed.auth_tag = src.substr(head_len + pnlen + payload_len, tag_len);
                if (parsed.head.size() != head_len + pnlen ||
                    parsed.protected_payload.size() != payload_len ||
                    parsed.auth_tag.size() != tag_len) {
                    return {{}, false};
                }
                if (src.size() != head_len + pnlen + payload_len + tag_len) {
                    return {{}, false};
                }
                return {parsed, true};
            }
        };

        namespace test {
            constexpr bool check_crypto() {
                // from https://tex2e.github.io/rfc-translater/html/rfc9001.html#A-5--ChaCha20-Poly1305-Short-Header-Packet
                byte data[]{
                    0x4c,                                                                                           // first_byte - short header
                    0xfe, 0x41, 0x89,                                                                               // packet number field
                    0x65,                                                                                           // protected_patload
                    0x5e, 0x5c, 0xd5, 0x5c, 0x41, 0xf6, 0x90, 0x80, 0x57, 0x5d, 0x79, 0x99, 0xc2, 0x5a, 0x5b, 0xfb  // auth_tag
                };
                auto tag = view::rvec(data + 5, 16);
                auto head = view::rvec(data, 1);
                auto head_with_pn = view::rvec(data, 4);
                CryptoPacket packet;
                // length of packet header without packet_number field is 1
                // (packet_number length is 3)
                packet.head_len = 1;
                packet.src = data;
                auto [parsed, ok] = packet.parse(16, 16);
                if (!ok) {
                    return false;
                }
                if (parsed.head != head ||
                    parsed.auth_tag != tag ||
                    parsed.sample != tag) {
                    return false;
                }
                auto [pnknown, ok2] = packet.parse_pnknown(3, 16);
                if (!ok2) {
                    return false;
                }
                return pnknown.head == head_with_pn &&
                       pnknown.auth_tag == tag;
            }

            static_assert(check_crypto());
        }  // namespace test

    }  // namespace dnet::quic::packet
}  // namespace utils

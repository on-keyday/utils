/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// handshake_0rtt - Handshake and 0RTT packet
#pragma once
#include "long_packet.h"
#include "long_plain_cipher.h"

namespace utils {
    namespace dnet::quic::packet {

        template <PacketType type>
        struct HandshakeZeroRTTParital : LongPacketBase {
            varint::Value length;  // only parse

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, type) &&
                       varint::read(r, length);
            }

            constexpr size_t len(bool use_length_field = false) const noexcept {
                return LongPacketBase::len() +
                       (use_length_field ? varint::len(length) : 0);
            }

            constexpr bool render(io::writer& w, byte pnlen) const noexcept {
                return render_with_pnlen(w, type, pnlen);
                // length field will be written by derived class
            }

            static constexpr size_t rvec_field_count() noexcept {
                return LongPacketBase::rvec_field_count();
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                LongPacketBase::visit_rvec(cb);
            }
            constexpr void visit_rvec(auto&& cb) const noexcept {
                LongPacketBase::visit_rvec(cb);
            }
        };

        using HandshakePacketParital = HandshakeZeroRTTParital<PacketType::Handshake>;
        using ZeroRTTPacketParital = HandshakeZeroRTTParital<PacketType::ZeroRTT>;

        using HandshakePacketPlain = LongPacketPlainBase<HandshakePacketParital>;
        using HandshakePacketCipher = LongPacketCipherBase<HandshakePacketParital>;

        using ZeroRTTPacketPlain = LongPacketPlainBase<ZeroRTTPacketParital>;
        using ZeroRTTPacketCipher = LongPacketCipherBase<ZeroRTTPacketParital>;
        namespace test {
            constexpr auto check_handshake_zerortt() {
                HandshakePacketPlain plain;
                byte tag[16] = "hello world! or";
                byte id[9] = "idididid";
                byte payload[11]{};  // dummy
                byte data[1210]{};
                auto [wire, ok] = packetnum::encode(1, 0);
                plain.version = 1;
                plain.srcID = id;
                plain.dstID = id;
                plain.wire_pn = wire.value;
                plain.payload = payload;
                plain.auth_tag = tag;
                io::writer w{data};
                if (!plain.render(w, wire.len)) {
                    return false;
                }
                io::reader r{w.written()};
                if (!plain.parse(r, 16)) {
                    return false;
                }
                ok = plain.flags.type() == PacketType::Handshake &&
                     plain.flags.packet_number_length() == wire.len &&
                     plain.version == 1 &&
                     plain.dstIDLen == 9 &&
                     plain.dstID == id &&
                     plain.srcIDLen == 9 &&
                     plain.srcID == id &&
                     plain.wire_pn == wire.value &&
                     plain.length == (wire.len + 11 + 16) &&
                     plain.auth_tag == tag;
                if (!ok) {
                    return false;
                }
                r.reset();
                HandshakePacketCipher cipher;
                if (!cipher.parse(r, 16)) {
                    return false;
                }
                ok = cipher.protected_payload.size() == wire.len + 11;
                return ok;
            }

            static_assert(check_handshake_zerortt());
        }  // namespace test
    }      // namespace dnet::quic::packet
}  // namespace utils

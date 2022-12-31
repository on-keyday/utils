/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// intial_packet - initial packet generation
#pragma once
#include "long_packet.h"
#include "long_plain_cipher.h"

namespace utils {
    namespace dnet::quic::packet {
        struct InitialPacketPartial : LongPacketBase {
            varint::Value token_length;  // only parse
            view::rvec token;
            varint::Value length;  // only parse

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, PacketType::Initial) &&
                       varint::read(r, token_length) &&
                       r.read(token, token_length) &&
                       varint::read(r, length);
            }

            constexpr size_t len(bool use_length_field = false) const noexcept {
                return LongPacketBase::len() +
                       (use_length_field
                            ? varint::len(token_length)
                            : varint::len(varint::Value(token.size()))) +
                       (use_length_field ? varint::len(length) : 0);
            }

            constexpr bool render(io::writer& w, byte pnlen) const noexcept {
                return render_with_pnlen(w, PacketType::Initial, pnlen) &&
                       varint::write(w, varint::Value(token.size())) &&
                       w.write(token);
                // length field will be written by derived class
            }

            static constexpr size_t rvec_field_count() noexcept {
                return LongPacketBase::rvec_field_count() +
                       1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                LongPacketBase::visit_rvec(cb);
                cb(token);
            }
            constexpr void visit_rvec(auto&& cb) const noexcept {
                LongPacketBase::visit_rvec(cb);
                cb(token);
            }
        };

        using InitialPacketPlain = LongPacketPlainBase<InitialPacketPartial>;
        using InitialPacketCipher = LongPacketCipherBase<InitialPacketPartial>;

        namespace test {
            constexpr auto check_initial() {
                InitialPacketPlain plain;
                byte tag[16] = "hello world! or";
                byte id[9] = "idididid";
                byte payload[11]{};  // dummy
                byte data[1210]{};
                auto [wire, ok] = packetnum::encode(1, 0);
                plain.version = 1;
                plain.srcID = id;
                plain.dstID = id;
                plain.token = id;
                plain.wire_pn = wire.value;
                plain.payload = payload;
                plain.auth_tag = tag;
                size_t normal = plain.len(false, wire.len);
                size_t padding = 1200 - normal;
                size_t with_padding = plain.len(false, wire.len, padding);
                padding -= with_padding - 1200;
                size_t fit_size = plain.len(false, wire.len, padding);
                if (fit_size != 1200) {
                    return false;
                }
                io::writer w{data};
                if (!plain.render(w, wire.len, padding)) {
                    return false;
                }
                io::reader r{w.written()};
                if (!plain.parse(r, 16)) {
                    return false;
                }
                ok = plain.flags.type() == PacketType::Initial &&
                     plain.flags.packet_number_length() == wire.len &&
                     plain.version == 1 &&
                     plain.dstIDLen == 9 &&
                     plain.dstID == id &&
                     plain.srcIDLen == 9 &&
                     plain.srcID == id &&
                     plain.token_length.value == 9 &&
                     plain.token == id &&
                     plain.wire_pn == wire.value &&
                     plain.length == (wire.len + 11 + padding + 16) &&
                     plain.auth_tag == tag;
                if (!ok) {
                    return false;
                }
                r.reset();
                InitialPacketCipher cipher;
                if (!cipher.parse(r, 16)) {
                    return false;
                }
                ok = cipher.protected_payload.size() == wire.len + 11 + padding;
                return ok;
            }

            static_assert(check_initial());
        }  // namespace test

    }  // namespace dnet::quic::packet
}  // namespace utils

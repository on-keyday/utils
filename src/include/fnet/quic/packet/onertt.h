/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// onertt - OneRTT packet
#pragma once
#include "packet.h"
#include "../packet_number.h"

namespace utils {
    namespace fnet::quic::packet {
        struct OneRTTPacketPartial : Packet {
            view::rvec dstID;

            static constexpr PacketType get_packet_type() {
                return PacketType::OneRTT;
            }

            // get_dstID_len is bool(io::reader b,size_t* len)
            constexpr bool parse(binary::reader& r, auto&& get_dstID_len) noexcept {
                size_t len = 0;
                return Packet::parse_check(r, PacketType::OneRTT) &&
                       get_dstID_len(r.peeker(), &len) &&
                       r.read(dstID, len);
            }

            constexpr size_t len() const noexcept {
                return Packet::len() + dstID.size();
            }

            constexpr size_t head_len() const noexcept {
                return len();
            }

            constexpr bool render(binary::writer& w, std::uint32_t version, byte pnlen, bool key, bool spin = false) const noexcept {
                auto val = make_packet_flags(version, PacketType::OneRTT, pnlen, spin, key);
                if (val.value == 0) {
                    return false;
                }
                return w.write(val.value, 1) &&
                       w.write(dstID);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return Packet::rvec_field_count() +
                       1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                Packet::visit_rvec(cb);
                cb(dstID);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                Packet::visit_rvec(cb);
                cb(dstID);
            }
        };

        struct OneRTTPacketPlain : OneRTTPacketPartial {
            std::uint32_t wire_pn = 0;  // only parse
            view::rvec payload;
            view::rvec auth_tag;  // only parse

            constexpr bool parse(binary::reader& r, size_t tag_len, auto&& get_dstID_len) noexcept {
                return OneRTTPacketPartial::parse(r, get_dstID_len) &&
                       packetnum::read(r, wire_pn, flags.packet_number_length()) &&
                       r.read(payload, r.remain().size() - tag_len) &&
                       r.read(auth_tag, tag_len);
            }

            constexpr size_t len(bool use_length_field = false, byte pnlen = 4, size_t auth_tag_len = 0, size_t padding = 0) const noexcept {
                return OneRTTPacketPartial::len() +
                       (use_length_field ? flags.packet_number_length() : pnlen) +
                       payload.size() +
                       (use_length_field ? auth_tag.size() : padding + auth_tag_len);
            }

            constexpr bool render(binary::writer& w, std::uint32_t version, packetnum::WireVal pn, bool key, size_t auth_tag_len, size_t padding = 0, bool spin = false) const noexcept {
                return OneRTTPacketPartial::render(w, version, pn.len, key, spin) &&
                       packetnum::write(w, pn) &&
                       w.write(0, padding) &&
                       w.write(payload) &&
                       w.write(0, auth_tag_len);
            }

            constexpr bool render_in_place(binary::writer& w, std::uint32_t version, packetnum::WireVal pn, bool key, auto&& payload_render, size_t auth_tag_len, size_t padding = 0, bool spin = false) {
                auto check_write = [&] {
                    auto rem = w.remain();
                    if (rem.size() < auth_tag_len) {
                        return false;  // not enough buffer
                    }
                    auto tmpw = binary::writer(rem.substr(0, rem.size() - auth_tag_len));
                    if (!payload_render(tmpw)) {
                        return false;
                    }
                    w.offset(tmpw.offset());
                    return true;
                };
                return OneRTTPacketPartial::render(w, version, pn.len, key, spin) &&
                       packetnum::write(w, pn) &&
                       w.write(0, padding) &&
                       check_write() &&
                       w.write(0, auth_tag_len);
            }
        };

        struct OneRTTPacketCipher : OneRTTPacketPartial {
            view::rvec protected_payload;
            view::rvec auth_tag;

            constexpr bool parse(binary::reader& r, size_t tag_len, auto&& get_dstID_len) noexcept {
                return OneRTTPacketPartial::parse(r, get_dstID_len) &&
                       r.read(protected_payload, r.remain().size() - tag_len) &&
                       r.read(auth_tag, tag_len);
            }

            constexpr size_t len() const noexcept {
                return OneRTTPacketPartial::len() +
                       protected_payload.size() +
                       auth_tag.size();
            }
        };

        namespace test {
            constexpr bool check_onertt() {
                OneRTTPacketPlain plain;
                byte id[] = {'h', 'e', 'l', 'l'};
                byte tag[16] = "kdkdkd";
                byte payload[10]{};
                byte data[100]{};
                plain.dstID = id;
                plain.payload = payload;
                plain.auth_tag = tag;
                binary::writer w{data};
                if (!plain.render(w, 1, {.value = 1, .len = 1}, false, 16)) {
                    return false;
                }
                binary::reader r{w.written()};
                if (!plain.parse(r, 16, [](binary::reader, size_t* len) {
                        *len = 4;
                        return true;
                    })) {
                    return false;
                }
                bool ok = plain.dstID == id &&
                          plain.payload == payload &&
                          plain.auth_tag.size() == 16;
                if (!ok) {
                    return false;
                }
                r.reset();
                OneRTTPacketCipher cipher;
                if (!cipher.parse(r, 16, [](binary::reader, size_t* len) {
                        *len = 4;
                        return true;
                    })) {
                    return false;
                }
                ok = plain.auth_tag.size() == 16;
                return ok;
            }

            static_assert(check_onertt());
        }  // namespace test

    }  // namespace fnet::quic::packet
}  // namespace utils

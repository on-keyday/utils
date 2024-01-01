/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// long_plain_cipher - long packet plain and cipher
#pragma once
#include "packet.h"
#include "../varint.h"
#include "../packet_number.h"

namespace futils {
    namespace fnet::quic::packet {

        template <class Partial>
        struct LongPacketPlainBase : Partial {
            std::uint32_t wire_pn;  // only parse
            view::rvec payload;
            view::rvec auth_tag;  // only parse

            constexpr bool parse(binary::reader& r, size_t tag_len) noexcept {
                return Partial::parse(r) &&
                       packetnum::read(r, wire_pn, this->flags.packet_number_length()) &&
                       r.read(payload, this->length.value - this->flags.packet_number_length() - tag_len) &&
                       r.read(auth_tag, tag_len);
            }

            // strictly, packet header includes packet_number field but
            // this tells us length of packet header without packet_number
            constexpr size_t head_len(bool use_length_field = false, byte pnlen = 4, size_t auth_tag_len = 0, size_t padding = 0) const noexcept {
                return Partial::len(use_length_field) +
                       (use_length_field
                            ? 0
                            : varint::len(pnlen + payload.size() + padding + auth_tag_len));
            }

            constexpr size_t len(bool use_length_field = false, byte pnlen = 4, size_t auth_tag_len = 0, size_t padding = 0) const noexcept {
                return head_len(use_length_field, pnlen, auth_tag_len, padding) +
                       (use_length_field ? this->flags.packet_number_length() : pnlen) +
                       payload.size() +
                       (use_length_field ? auth_tag.size() : padding + auth_tag_len);
            }

            constexpr bool render(binary::writer& w, packetnum::WireVal pn, size_t auth_tag_len, size_t padding = 0) const noexcept {
                return Partial::render(w, pn.len) &&
                       varint::write(w, pn.len + payload.size() + padding + auth_tag_len) &&
                       packetnum::write(w, pn) &&
                       w.write(0, padding) &&
                       w.write(payload) &&
                       w.write(0, auth_tag_len);
            }

            constexpr bool render_in_place(binary::writer& w, packetnum::WireVal pn, auto&& payload_render, size_t auth_tag_len, size_t padding = 0) {
                auto write_length_pn_payload_tag = [&] {
                    auto rem = w.remain();
                    const auto min_ = varint::len(pn.len + padding + auth_tag_len);
                    const auto max_ = varint::len(rem.size());
                    if (min_ > max_) {
                        return false;
                    }
                    const auto reserved = max_ + pn.len + padding + auth_tag_len;
                    if (reserved > rem.size()) {
                        return false;
                    }
                    const auto usable = rem.size() - reserved;
                    auto tmpw = binary::writer(rem.substr(reserved, usable));
                    if (!payload_render(tmpw)) {
                        return false;
                    }
                    const auto written_size = tmpw.offset();
                    const auto length_filed_value = pn.len + written_size + padding + auth_tag_len;
                    const auto pre_reserved = varint::len(length_filed_value) + pn.len + padding;
                    if (!varint::write(w, length_filed_value) ||  // length field
                        !packetnum::write(w, pn) ||               // packet_nubmer field
                        !w.write(0, padding)) {                   // padding
                        return false;
                    }
                    if (!view::shift(rem, pre_reserved, reserved, written_size)) {
                        return false;
                    }
                    w.offset(written_size);
                    return w.write(0, auth_tag_len);
                };
                return Partial::render(w, pn.len) &&
                       write_length_pn_payload_tag();
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                Partial::visit_rvec(cb);
                cb(payload);
                cb(auth_tag);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                Partial::visit_rvec(cb);
                cb(payload);
                cb(auth_tag);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return Partial::rvec_field_count() +
                       2;
            }
        };

        // not implement render on ~Cipher class
        // because ~Cipher can't know packet number and its length
        template <class Partial>
        struct LongPacketCipherBase : Partial {
            view::rvec protected_payload;
            view::rvec auth_tag;
            constexpr bool parse(binary::reader& r, size_t tag_len) noexcept {
                return Partial::parse(r) &&
                       r.read(protected_payload, this->length.value - tag_len) &&
                       r.read(auth_tag, tag_len);
            }

            constexpr size_t head_len() const noexcept {
                return Partial::len(true);
            }

            constexpr size_t len() const noexcept {
                return head_len() +
                       protected_payload.size() +
                       auth_tag.size();
            }

            static constexpr size_t rvec_field_count() noexcept {
                return Partial::rvec_field_count() +
                       2;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                Partial::visit_rvec(cb);
                cb(protected_payload);
                cb(auth_tag);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                Partial::visit_rvec(cb);
                cb(protected_payload);
                cb(auth_tag);
            }
        };
    }  // namespace fnet::quic::packet
}  // namespace futils

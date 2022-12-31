/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// long_plain_cipher - long packet plain and cipher
#pragma once
#include "packet.h"
#include "../varint.h"
#include "../packet_number.h"

namespace utils {
    namespace dnet::quic::packet {

        template <class Partial>
        struct LongPacketPlainBase : Partial {
            std::uint32_t wire_pn;
            view::rvec payload;
            view::rvec auth_tag;

            constexpr bool parse(io::reader& r, size_t tag_len) noexcept {
                return Partial::parse(r) &&
                       packetnum::read(r, wire_pn, this->flags.packet_number_length()) &&
                       r.read(payload, this->length.value - this->flags.packet_number_length() - tag_len) &&
                       r.read(auth_tag, tag_len);
            }

            constexpr size_t head_len(bool use_length_field = false, byte pnlen = 4, size_t padding = 0) const noexcept {
                return Partial::len(use_length_field) +
                       (use_length_field
                            ? 0
                            : varint::len(pnlen + payload.size() + padding + auth_tag.size()));
            }

            constexpr size_t len(bool use_length_field = false, byte pnlen = 4, size_t padding = 0) const noexcept {
                return head_len(use_length_field, pnlen, padding) +
                       (use_length_field ? this->flags.packet_number_length() : pnlen) +
                       payload.size() +
                       (use_length_field ? 0 : padding) +
                       auth_tag.size();
            }

            constexpr bool render(io::writer& w, byte pnlen, size_t padding = 0) const noexcept {
                return Partial::render(w, pnlen) &&
                       varint::write(w, pnlen + payload.size() + padding + auth_tag.size()) &&
                       packetnum::write(w, packetnum::WireVal{.value = wire_pn, .len = pnlen}) &&
                       w.write(payload) &&
                       w.write(0, padding) &&
                       w.write(auth_tag);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return Partial::rvec_field_count() +
                       2;
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
        };

        // not implement render on ~Cipher class
        // because Cipher can't know packet number
        // this design
        template <class Partial>
        struct LongPacketCipherBase : Partial {
            view::rvec protected_payload;
            view::rvec auth_tag;
            constexpr bool parse(io::reader& r, size_t tag_len) noexcept {
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
    }  // namespace dnet::quic::packet
}  // namespace utils

/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stateless_reset - StatelessReset
#pragma once
#include "packet.h"

namespace futils {
    namespace fnet::quic::packet {
        struct StatelessReset : Packet {
            view::rvec unpredicable_bits;
            byte stateless_reset_token[16];

            static constexpr PacketType get_packet_type() {
                return PacketType::StatelessReset;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return Packet::parse(r) &&
                       flags.is_short() &&
                       !flags.invalid() &&
                       r.read(unpredicable_bits, r.remain().size() - 16) &&
                       r.read(stateless_reset_token);
            }

            constexpr size_t len() const {
                return Packet::len() +
                       unpredicable_bits.size() +
                       16;
            }

            constexpr bool render(binary::writer& w, byte first_byte_random = 0) const {
                if (unpredicable_bits.size() < 4) {
                    return false;
                }
                return w.write(0x40 | (0x3f & first_byte_random), 1) &&
                       w.write(unpredicable_bits) &&
                       w.write(stateless_reset_token);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(unpredicable_bits);
            }
            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(unpredicable_bits);
            }
        };
        namespace test {
            constexpr bool check_stateless_reset() {
                StatelessReset reset;
                byte bits[10] = "hogereya";
                byte data[100];
                binary::writer w{data};
                reset.unpredicable_bits = bits;
                byte tok[] = "hogehoge ieyona";
                view::copy(reset.stateless_reset_token, tok);
                if (!reset.render(w, 'm')) {
                    return false;
                }
                binary::reader r{w.written()};
                if (!reset.parse(r)) {
                    return false;
                }
                return reset.unpredicable_bits == bits &&
                       reset.stateless_reset_token == view::rvec(tok);
            }

            static_assert(check_stateless_reset());
        }  // namespace test
    }      // namespace fnet::quic::packet
}  // namespace futils

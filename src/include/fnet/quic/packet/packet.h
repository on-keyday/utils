/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../types.h"
#include "../../../binary/reader.h"
#include "../../../binary/writer.h"

namespace futils {
    namespace fnet::quic::packet {

        struct Packet {
            PacketFlags flags;  // only parse
            constexpr bool parse(binary::reader& r) noexcept {
                if (r.empty()) {
                    return false;
                }
                flags = {r.top()};
                r.offset(1);
                return true;
            }

            constexpr bool parse_check(binary::reader& r, PacketType type) noexcept {
                if (!parse(r)) {
                    return false;
                }
                if (is_LongPacket(type) || type == PacketType::VersionNegotiation) {
                    return flags.is_long();
                }
                else if (type == PacketType::OneRTT) {
                    return flags.is_short();
                }
                return true;  // skip check
            }

            constexpr size_t len() const noexcept {
                return 1;
            }

            constexpr bool render(binary::writer& r) const noexcept {
                return r.write(flags.value, 1);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 0;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {}
            constexpr void visit_rvec(auto&& cb) const noexcept {}
        };

    }  // namespace fnet::quic::packet
}  // namespace futils

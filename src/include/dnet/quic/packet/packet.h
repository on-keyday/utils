/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../types.h"
#include "../../../io/reader.h"
#include "../../../io/writer.h"

namespace utils {
    namespace dnet::quic::packet {

        struct Packet {
            PacketFlags flags;  // only parse
            constexpr bool parse(io::reader& r) noexcept {
                if (r.empty()) {
                    return false;
                }
                flags = {r.top()};
                r.offset(1);
                return true;
            }

            constexpr bool parse_check(io::reader& r, PacketType type) noexcept {
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

            constexpr bool render(io::writer& r) const noexcept {
                return r.write(flags.value, 1);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 0;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {}
            constexpr void visit_rvec(auto&& cb) const noexcept {}
        };

    }  // namespace dnet::quic::packet
}  // namespace utils

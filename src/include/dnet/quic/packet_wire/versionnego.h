/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// versionnego - version negotiation packet
#pragma once
#include "long_packet.h"
#include "../test.h"

namespace utils {
    namespace dnet::quic::packet {
        template <template <class...> class Vec>
        struct VersionNegotiationPacket : LongPacketBase {
            Vec<std::uint32_t> negotiated_versions;
            constexpr bool parse(io::reader& r) {
                if (!parse_check(r, PacketType::VersionNegotiation)) {
                    return false;
                }
                negotiated_versions.clear();
                while (!r.empty()) {
                    std::uint32_t ver;
                    if (!io::read_num(r, ver, true)) {
                        return false;
                    }
                    negotiated_versions.push_back(ver);
                }
                return true;
            }

            constexpr size_t len() const noexcept {
                return LongPacketBase::len() +
                       4 * negotiated_versions.size();
            }

            constexpr bool render(io::writer& w) const noexcept {
                auto val = flags;
                if (!flags.is_long()) {
                    val = make_packet_flags(0, PacketType::VersionNegotiation, 0);
                }
                if (!render_with_version(w, val, 0)) {
                    return false;
                }
                for (std::uint32_t ver : negotiated_versions) {
                    if (!io::write_num(w, ver, true)) {
                        return false;
                    }
                }
                return true;
            }
        };

        namespace test {
            constexpr bool check_version_negotiation() {
                VersionNegotiationPacket<quic::test::FixedTestVec> packet;
                packet.negotiated_versions.push_back(9292);
                packet.negotiated_versions.push_back(293321);
                packet.negotiated_versions.push_back(9394);
                byte data[100];
                byte dummy[] = {'d', 'u', 'm', 'o', 'm'};
                io::writer w{data};
                packet.dstID = dummy;
                packet.srcID = dummy;
                if (!packet.render(w)) {
                    return false;
                }
                io::reader r{w.written()};
                if (!packet.parse(r)) {
                    return false;
                }
                return packet.srcID == dummy &&
                       packet.dstID == dummy &&
                       packet.negotiated_versions[0] == 9292 &&
                       packet.negotiated_versions[1] == 293321 &&
                       packet.negotiated_versions[2] == 9394;
            }

            static_assert(check_version_negotiation());
        }  // namespace test
    }      // namespace dnet::quic::packet
}  // namespace utils

/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// long_packet - long packet base
#pragma once
#include "packet.h"
#include "../../../binary/number.h"
#include "../packet_number.h"

namespace futils {
    namespace fnet::quic::packet {
        struct LongPacketBase : Packet {
            std::uint32_t version = 0;
            byte dstIDLen = 0;  // only parse
            view::rvec dstID;
            byte srcIDLen = 0;  // only parse
            view::rvec srcID;

            constexpr bool parse(binary::reader& r) noexcept {
                return Packet::parse(r) &&
                       flags.is_long() &&
                       binary::read_num(r, version, true) &&
                       r.read(view::wvec(&dstIDLen, 1)) &&
                       r.read(dstID, dstIDLen) &&
                       r.read(view::wvec(&srcIDLen, 1)) &&
                       r.read(srcID, srcIDLen);
            }

            constexpr bool parse_check(binary::reader& r, PacketType type) noexcept {
                if (!parse(r)) {
                    return false;
                }
                if (is_LongPacket(type)) {
                    return flags.type(version) == type;
                }
                else if (type == PacketType::VersionNegotiation) {
                    return version == 0;
                }
                return true;
            }

            constexpr size_t len() const noexcept {
                return Packet::len() +
                       4 +
                       1 +
                       dstID.size() +
                       1 +
                       srcID.size();
            }

            constexpr bool render_with_version(binary::writer& w, PacketFlags first_byte, std::uint32_t ver) const noexcept {
                if (dstID.size() > 0xff ||
                    srcID.size() > 0xff) {
                    return false;
                }
                return w.write(first_byte.value, 1) &&
                       binary::write_num(w, ver, true) &&
                       w.write(byte(dstID.size()), 1) &&
                       w.write(dstID) &&
                       w.write(byte(srcID.size()), 1) &&
                       w.write(srcID);
            }

            constexpr bool render_with_pnlen(binary::writer& w, PacketType type, byte pnlen) const noexcept {
                if (!packetnum::is_wire_len(pnlen)) {
                    return false;
                }
                auto val = make_packet_flags(version, type, pnlen);
                if (val.value == 0) {
                    return false;  // 0 is invalid. maybe version is invalid
                }
                return render_with_version(w, val, version);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return render_with_version(w, flags, version);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 2;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(dstID);
                cb(srcID);
            }
            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(dstID);
                cb(srcID);
            }
        };
    }  // namespace fnet::quic::packet
}  // namespace futils

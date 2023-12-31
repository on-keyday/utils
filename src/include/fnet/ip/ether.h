/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../binary/number.h"
#include "../util/crc.h"

namespace utils {
    namespace fnet::ether {

        struct EthernetFrame {
            byte dst_mac[6]{};
            byte src_mac[6]{};
            bool has_vlan_tag = false;
            std::uint16_t vlan_tag = 0;  // optional
            std::uint16_t ether_type = 0;
            view::rvec payload;

            constexpr bool parse(view::rvec frame) noexcept {
                binary::reader r{frame};
                if (!r.read(dst_mac) ||
                    !r.read(src_mac) ||
                    !binary::read_num(r, ether_type)) {
                    return false;
                }
                if (ether_type == 0x8100) {
                    if (!binary::read_num(r, vlan_tag)) {
                        return false;
                    }
                    has_vlan_tag = true;
                    if (!binary::read_num(r, ether_type)) {
                        return false;
                    }
                }
                auto len = r.remain().size() - 4;
                if (ether_type <= 1500) {
                    len = ether_type;
                }
                return r.read(payload, len) &&
                       r.empty();
            }

            constexpr bool render(binary::writer& w) const noexcept {
                const auto ofs = w.offset();
                if (!w.write(dst_mac) ||
                    !w.write(src_mac)) {
                    return false;
                }
                if (has_vlan_tag) {
                    if (!binary::write_num(w, std::uint16_t(0x8100)) ||
                        !binary::write_num(w, vlan_tag)) {
                        return false;
                    }
                }
                std::uint16_t to_write = 0;
                if (ether_type >= 1536) {
                    to_write = ether_type;
                }
                else if (ether_type <= 1500) {
                    if (payload.size() > 1500) {
                        return false;
                    }
                    to_write = std::uint16_t(payload.size());
                }
                else {
                    return false;
                }
                return binary::write_num(w, to_write) &&
                       binary::write_num(w, payload);
            }
        };

        struct EthernetFrameWithCRC {
            EthernetFrame frame;
            std::uint32_t crc = 0;
            constexpr bool parse(view::rvec f) noexcept {
                auto crc_target = f.substr(0, f.size() - 4);
                if (!frame.parse(crc_target)) {
                    return false;
                }
                binary::reader r{f.substr(crc_target.size())};
                if (!binary::read_num(r, crc) &&
                    !r.empty()) {
                    return false;
                }
                if (crc != ~crc::crc32(crc_target)) {
                    return false;
                }
                return true;
            }

            constexpr bool render(binary::writer& w) const noexcept {
                const auto offset = w.offset();
                if (!frame.render(w)) {
                    return false;
                }
                return binary::write_num(w, crc::crc32(w.written().substr(offset)));
            }
        };

        namespace test {
            constexpr bool check_ether_frame() {
                EthernetFrame frame;
                return true;
            }

            static_assert(check_ether_frame());
        }  // namespace test
    }      // namespace fnet::ether
}  // namespace utils

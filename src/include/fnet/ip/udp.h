/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/number.h>
#include "ip.h"

namespace futils::fnet::udp {
    struct UDPPacket {
        std::uint16_t src_port = 0;
        std::uint16_t dst_port = 0;
        std::uint16_t length = 0;
        std::uint16_t check_sum = 0;
        view::rvec data;

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(r, true, src_port, dst_port, length, check_sum) &&
                   r.read(data, length);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            if (data.size() > 0xffff) {
                return false;
            }
            return binary::write_num_bulk(w, true, src_port, dst_port, std::uint16_t(data.size()), check_sum) &&
                   w.write(data);
        }

        // use length field instead of data.size()
        constexpr bool render_header(binary::writer& w) const noexcept {
            return binary::write_num_bulk(w, true, src_port, dst_port, length, check_sum);
        }

        constexpr bool render_with_checksum(binary::writer& w, std::uint16_t pseudo_hdr_chksum) const noexcept {
            auto pkt = *this;   // copy
            pkt.check_sum = 0;  // fill zero
            auto cur = w.offset();
            if (!render(w)) {
                return false;
            }
            auto size = w.offset() - cur;
            auto data = w.written().substr(cur, size);
            ip::checksum(data, pseudo_hdr_chksum, true).transform([&](std::uint16_t code) {
                binary::writer w{data};
                w.offset(6);
                binary::write_num(w, code);  // write checksum
            });
            return true;
        }
    };

}  // namespace futils::fnet::udp

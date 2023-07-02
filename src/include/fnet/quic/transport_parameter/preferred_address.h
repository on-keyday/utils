/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// preferred_address - QUIC transport parameter preferred address
#pragma once
#include "../../../binary/number.h"

namespace utils {
    namespace fnet::quic::trsparam {
        struct PreferredAddress {
            byte ipv4_address[4]{};
            std::uint16_t ipv4_port = 0;
            byte ipv6_address[16]{};
            std::uint16_t ipv6_port{};
            byte connectionID_length;  // only parse
            view::rvec connectionID;
            byte stateless_reset_token[16];

            constexpr bool parse(binary::reader& r) {
                return r.read(ipv4_address) &&
                       binary::read_num(r, ipv4_port, true) &&
                       r.read(ipv6_address) &&
                       binary::read_num(r, ipv6_port, true) &&
                       r.read(view::wvec(&connectionID_length, 1)) &&
                       r.read(connectionID, connectionID_length) &&
                       r.read(stateless_reset_token);
            }

            constexpr size_t len() const {
                return 4 + 2 + 16 + 2 +
                       1 +  // length
                       connectionID.size() + 16;
            }

            constexpr bool render(binary::writer& w) const {
                if (connectionID.size() > 0xff) {
                    return false;
                }
                return w.write(ipv4_address) &&
                       binary::write_num(w, ipv4_port, true) &&
                       w.write(ipv6_address) &&
                       binary::write_num(w, ipv6_port, true) &&
                       w.write(byte(connectionID.size()), 1) &&
                       w.write(connectionID) &&
                       w.write(stateless_reset_token);
            }
        };

        using enough_to_store_preferred_address =
            byte[4 + 2 + 16 + 2 + 1 + 20 + 16];

    }  // namespace fnet::quic::trsparam
}  // namespace utils

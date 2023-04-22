/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../address.h"
#include "../transport_parameter/preferred_address.h"

namespace utils {
    namespace fnet::quic::path {

        enum class MigrateMode {
            none,
            migrate_address,
            migrate_port,
        };

        constexpr MigrateMode compare_address(const NetAddrPort& a, const NetAddrPort& b) {
            if (a.addr.type() != b.addr.type()) {
                return MigrateMode::migrate_address;
            }
            auto cmp1 = view::rvec(a.addr);
            auto cmp2 = view::rvec(b.addr);
            if (cmp1 != cmp2) {
                return MigrateMode::migrate_address;
            }
            if (a.port != a.port) {
                return MigrateMode::migrate_port;
            }
            return MigrateMode::none;
        }

        struct PeerAddr {
           private:
            NetAddrPort current_addr;
            NetAddrPort migrate_addr;
            MigrateMode mode = MigrateMode::none;

           public:
            constexpr bool is_current_addr(const NetAddrPort& np) const {
                return compare_address(current_addr, np) == MigrateMode::none;
            }

            const NetAddrPort& get_current_addr() const {
                return current_addr;
            }

            const NetAddrPort& get_migrate_addr() const {
                return current_addr;
            }

            constexpr MigrateMode current_mode() const {
                return mode;
            }

            void reset(NetAddrPort&& peer) {
                current_addr = std::move(peer);
                migrate_addr = {};
                mode = MigrateMode::none;
            }

            bool is_not_received() const {
                return current_addr.addr.type() == NetAddrType::null;
            }

            void on_packet_decrypted(NetAddrPort&& recv_addr) {
                mode = compare_address(current_addr, recv_addr);
                if (mode != MigrateMode::none) {
                    migrate_addr = std::move(recv_addr);
                }
            }
        };

    }  // namespace fnet::quic::path
}  // namespace utils

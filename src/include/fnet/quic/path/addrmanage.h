/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../address.h"
#include "../transport_parameter/preferred_address.h"
#include "path.h"
#include "../../std/hash_map.h"
#include "../../../view/iovec.h"

namespace utils {
    namespace fnet::quic::path::ip {
        struct RawPathKey {
            byte data[36]{};

            friend constexpr bool operator==(const RawPathKey& a, const RawPathKey& b) {
                return view::rvec(a.data, 36) == view::rvec(b.data, 36);
            }
        };
    }  // namespace fnet::quic::path::ip
}  // namespace utils

namespace std {
    template <>
    struct hash<utils::fnet::quic::path::ip::RawPathKey> {
        constexpr auto operator()(auto&& d) const {
            return utils::view::hash(d.data);
        }
    };

    template <>
    struct hash<utils::fnet::quic::path::PathID> {
        constexpr auto operator()(auto&& d) const {
            return std::hash<std::uint32_t>{}(d.id);
        }
    };

}  // namespace std

namespace utils {
    namespace fnet::quic::path::ip {

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
            if (a.port() != a.port()) {
                return MigrateMode::migrate_port;
            }
            return MigrateMode::none;
        }

        constexpr RawPathKey ip_key(const NetAddrPort& local, const NetAddrPort& peer) {
            RawPathKey d;
            auto i = 16 - local.addr.size();
            auto j = 16 - peer.addr.size();
            for (; i < local.addr.size(); i++) {
                d.data[i] = local.addr.data()[i];
            }
            binary::writer w{view::wvec(d.data + 16, 2)};
            binary::write_num(w, local.port().u16());
            for (; j < peer.addr.size(); j++) {
                d.data[18 + j] = peer.addr.data()[i];
            }
            w.reset(view::wvec(d.data + 34, 2));
            binary::write_num(w, peer.port().u16());
            return d;
        }

        struct PathAddr {
            PathID id;
            NetAddrPort local;
            NetAddrPort peer;
        };

        struct PathMapper {
            slib::hash_map<RawPathKey, PathAddr> ingress;
            slib::hash_map<PathID, PathAddr*> egress;
            NetAddrPort active_local;
            PathID id = original_path;

            PathID get_path_id(const NetAddrPort& peer) {
                auto key = ip_key(active_local, peer);
                auto found = ingress.find(key);
                if (found != ingress.end()) {
                    return found->second.id;
                }
                auto [it, ok] = ingress.emplace(key, PathAddr{
                                                         .id = id,
                                                         .local = active_local,
                                                         .peer = peer,
                                                     });
                egress.emplace(id, &it->second);
                auto g = id;
                id.id++;
                return g;
            }

            NetAddrPort get_peer_address(PathID id) const {
                auto found = egress.find(id);
                if (found == egress.end()) {
                    return {};
                }
                return found->second->peer;
            }
        };

    }  // namespace fnet::quic::path::ip
}  // namespace utils

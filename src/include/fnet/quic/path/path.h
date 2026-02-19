/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>

namespace futils {
    namespace fnet::quic::path {
        // PathID is unique id for each path
        // this type is library sepecific type so that not explained in RFC
        // note: Path is a pair of source address and destination address.
        // addresses mean IP address and UDP port.
        // if we use IPv6, Path pattern is simply
        // patternof((16 byte +2 byte) * 2 = 36 byte) = 4.9732323640978664e+86
        // so this class is not enough to represent all Path space
        // but number of addresses used for a single connection is maybe limited enough so using uint32_t to represent path address instead using raw IP address.
        // user should map IP address to PathID.
        // by this mechanism, this QUIC implementation is independent from low layer addressing details.
        struct PathID {
            std::uint32_t id = ~0;
            constexpr PathID() = default;
            constexpr PathID(std::uint32_t v)
                : id(v) {}

            constexpr operator std::uint32_t() const noexcept {
                return id;
            }

            friend constexpr bool operator==(PathID lhs, PathID rhs) noexcept {
                return lhs.id == rhs.id;
            }
        };

        // original_path is id for path that is used for handshake
        constexpr PathID original_path = 0;
        // preferred_path is id for path provided by preferred address transport parameter
        constexpr PathID preferred_path = 1;

        constexpr bool is_reserved_path(PathID id) noexcept {
            return original_path == id || preferred_path == id;
        }

        constexpr PathID migrate_path_begin = 2;
        // unknown_path is unknown path
        // place holder of unspecified
        constexpr PathID unknown_path = 0xffffffff;

    }  // namespace fnet::quic::path
}  // namespace futils

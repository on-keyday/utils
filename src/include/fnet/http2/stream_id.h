/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>

namespace futils {
    namespace fnet::http2::stream {
        struct ID {
            std::int32_t id = -1;

            constexpr ID() = default;

            constexpr ID(std::int32_t v)
                : id(v) {}

            constexpr bool valid() const {
                return id >= 0;
            }

            constexpr operator std::int32_t() const {
                return id;
            }

            constexpr bool is_conn() const {
                return id == 0;
            }

            constexpr bool by_client() const {
                return valid() &&
                       !is_conn() &&
                       (id & 0x1) == 1;
            }

            constexpr bool by_server() const {
                return valid() &&
                       !is_conn() &&
                       (id & 0x1) == 0;
            }

            constexpr ID next() const {
                if (!valid() || is_conn()) {
                    return -1;
                }
                return id + 2;
            }

            constexpr void unset_reserved() {
                id = std::uint32_t(id) & 0x7fffffff;
            }
        };

        constexpr auto invalid_id = ID(-1);
        constexpr auto conn_id = ID(0);
        constexpr auto client_first = ID(1);
        constexpr auto server_first = ID(2);
    }  // namespace fnet::http2::stream
}  // namespace futils

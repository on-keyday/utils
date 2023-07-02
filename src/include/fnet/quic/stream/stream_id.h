/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>

namespace utils {
    namespace fnet::quic::stream {
        // initiated by
        enum class Origin {
            client,
            server,
            unknown,
        };

        enum class StreamType {
            uni,
            bidi,
            unknown,
        };

        constexpr size_t dir_to_mask(Origin dir) {
            switch (dir) {
                case Origin::client:
                    return 0x0;
                case Origin::server:
                    return 0x1;
                default:
                    return ~0;
            }
        }

        constexpr Origin inverse(Origin dir) {
            switch (dir) {
                case Origin::client:
                    return Origin::server;
                case Origin::server:
                    return Origin::client;
                default:
                    return Origin::unknown;
            }
        }

        constexpr size_t type_to_mask(StreamType typ) {
            switch (typ) {
                case StreamType::uni:
                    return 0x2;
                case StreamType::bidi:
                    return 0x0;
                default:
                    return ~0;
            }
        }

        struct StreamID {
            std::uint64_t id = ~0;

            constexpr StreamID() = default;

            constexpr StreamID(std::uint64_t id)
                : id(id) {}

            constexpr bool valid() const {
                return id < (std::uint64_t(0x3) << 62);
            }

            constexpr operator std::uint64_t() const {
                return id;
            }

            constexpr std::uint64_t seq_count() const {
                if (!valid()) {
                    return ~0;
                }
                return id >> 2;
            }

            constexpr StreamType type() const {
                if (!valid()) {
                    return StreamType::unknown;
                }
                if (id & 0x2) {
                    return StreamType::uni;
                }
                else {
                    return StreamType::bidi;
                }
            }

            constexpr Origin dir() const {
                if (!valid()) {
                    return Origin::unknown;
                }
                if (id & 0x1) {
                    return Origin::server;
                }
                else {
                    return Origin::client;
                }
            }
        };

        constexpr StreamID make_id(std::uint64_t seq, Origin dir, StreamType type) noexcept {
            return (seq << 2) | type_to_mask(type) | dir_to_mask(dir);
        }

        constexpr StreamID invalid_id = ~0;
    }  // namespace fnet::quic::stream
}  // namespace utils

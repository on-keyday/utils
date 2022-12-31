/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_number - packet number
#pragma once
#include <cstdint>

namespace utils {
    namespace dnet::quic::ack {
        struct PacketNumber {
            std::uint64_t value = 0;

            constexpr PacketNumber(std::uint64_t v) noexcept
                : value(v) {}

            constexpr operator std::uint64_t() const noexcept {
                return value;
            }

            // constexpr friend auto operator<=>(const PacketNumber&, const PacketNumber&) noexcept = default;
        };

        constexpr static PacketNumber infinity = {~std::uint64_t(0)};

    }  // namespace dnet::quic::ack
}  // namespace utils

namespace std {
    template <class T>
    struct hash;

    template <>
    struct hash<utils::dnet::quic::ack::PacketNumber> {
        constexpr size_t operator()(auto&& pn) const {
            return std::hash<size_t>{}(pn.value);
        }
    };
}  // namespace std

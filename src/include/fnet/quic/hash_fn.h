/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hash_fn - std::hash
#pragma once
#include <functional>
#include "packet_number.h"
#include "stream/stream_id.h"
#include <string_view>

namespace std {

    template <>
    struct hash<utils::fnet::quic::packetnum::Value> {
        constexpr size_t operator()(auto pn) const {
            return std::hash<std::uint64_t>{}(pn.value);
        }
    };

    template <>
    struct hash<utils::fnet::quic::stream::StreamID> {
        constexpr auto operator()(auto id) const noexcept {
            return std::hash<decltype(id.id)>{}(id.id);
        }
    };

    template <>
    struct hash<utils::view::rvec> {
        constexpr auto operator()(auto id) const noexcept {
            return std::hash<std::string_view>{}(std::string_view(id.as_char(), id.size()));
        }
    };
}  // namespace std

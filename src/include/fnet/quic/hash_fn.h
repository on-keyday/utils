/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hash_fn - std::hash
#pragma once
#include <functional>
#include "packet_number.h"
#include "stream/stream_id.h"
#include "../storage.h"
#include <string_view>

namespace std {

    template <>
    struct hash<futils::fnet::quic::packetnum::Value> {
        constexpr size_t operator()(auto pn) const {
            return std::hash<decltype(pn.as_uint())>{}(pn.as_uint());
        }
    };

    template <>
    struct hash<futils::fnet::quic::stream::StreamID> {
        constexpr auto operator()(auto id) const noexcept {
            return std::hash<std::uint64_t>{}(id);
        }
    };

    template <>
    struct hash<futils::view::rvec> {
        constexpr auto operator()(auto id) const noexcept {
            return std::hash<std::string_view>{}(std::string_view(id.as_char(), id.size()));
        }
    };

    template <>
    struct hash<futils::fnet::flex_storage> {
        constexpr auto operator()(auto&& fx) const noexcept {
            return std::hash<futils::view::rvec>{}(fx);
        }
    };
}  // namespace std

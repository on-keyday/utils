/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../quic/varint.h"

namespace utils {
    namespace fnet::http3::unistream {
        enum class Type {
            CONTROL = 0x00,
            PUSH = 0x01,
            QPACK_ENCODER = 0x02,
            QPACK_DECODER = 0x03,
        };

        struct UniStreamHeader {
            quic::varint::Value type;

            constexpr bool parse(io::reader& r) {
                return quic::varint::read(r, type);
            }

            constexpr bool render(io::writer& w) const {
                return quic::varint::write(w, type);
            }

            constexpr bool parse_type(io::reader& r, Type t) {
                return parse(r) &&
                       type == std::uint64_t(t);
            }

            constexpr bool redner_type(io::writer& w, Type type) const {
                return quic::varint::write(w, std::uint64_t(type));
            }
        };
    }  // namespace fnet::http3::unistream
}  // namespace utils

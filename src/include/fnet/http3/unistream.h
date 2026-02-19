/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../quic/varint.h"

namespace futils {
    namespace fnet::http3::unistream {
        enum class Type {
            CONTROL = 0x00,
            PUSH = 0x01,
            QPACK_ENCODER = 0x02,
            QPACK_DECODER = 0x03,
        };

        struct UniStreamHeader {
            quic::varint::Value type;

            constexpr bool parse(binary::reader& r) {
                return quic::varint::read(r, type);
            }

            constexpr bool render(binary::writer& w) const {
                return quic::varint::write(w, type);
            }

            constexpr bool parse_type(binary::reader& r, Type t) {
                return parse(r) &&
                       type == std::uint64_t(t);
            }

            constexpr bool redner_type(binary::writer& w, Type type) const {
                return quic::varint::write(w, std::uint64_t(type));
            }
        };

        using UniStreamHeaderArea = byte[16];

        inline view::wvec get_header(UniStreamHeaderArea& area, Type type) {
            UniStreamHeader head;
            head.type = std::uint64_t(type);
            binary::writer w{area};
            if (!head.render(w)) {
                return {};
            }
            return w.written();
        }
    }  // namespace fnet::http3::unistream
}  // namespace futils

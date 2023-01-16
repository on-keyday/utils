/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame_base - QUIC frame base struct
#pragma once
#include "../../../bytelen.h"

namespace utils {
    namespace dnet {
        namespace quic::frame {

            constexpr auto nop_visitor() {
                return [](const ByteLen&) {};
            }

            struct Frame {
                FrameFlags type;

                constexpr bool parse(ByteLen& b) {
                    QVarInt qv;
                    if (!qv.parse(b) || !qv.is_minimum()) {
                        return false;
                    }
                    type = {qv.value};
                    return true;
                }

                constexpr size_t parse_len() const {
                    return render_len();
                }

                constexpr bool render(WPacket& w) const {
                    QVarInt qv{type.value};
                    return qv.render(w);
                }

                constexpr size_t render_len() const {
                    QVarInt qv{type.value};
                    return qv.minimum_len();
                }

                // visit_bytelen is visitor for bytelen
                // this is used for bytelen replace with heap allocated
                constexpr size_t visit_bytelen(auto&& cb) {
                    return 0;
                }

                constexpr size_t visit_bytelen(auto&& cb) const {
                    return 0;
                }
            };

        }  // namespace quic::frame
    }      // namespace dnet
}  // namespace utils

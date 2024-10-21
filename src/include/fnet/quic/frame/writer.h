/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// writer - frame writer
#pragma once
#include "../packet/summary.h"
#include "typeonly.h"

namespace futils {
    namespace fnet::quic::frame {
        struct fwriter {
            binary::writer& w;
            packet::PacketStatus status;
            FrameType prev_type = FrameType::PADDING;

            constexpr view::wvec remain() {
                return w.remain();
            }

            constexpr bool write(const auto& frame) noexcept {
                prev_type = frame.get_type();
                if (!frame.render(w)) {
                    return false;
                }
                status.add_frame(frame.get_type());
                return true;
            }
        };

        inline void add_padding_for_encryption(frame::fwriter& fw, packetnum::WireVal wire, size_t auth_tag) {
            // least 4 byte needed for sample skip size
            // see https://datatracker.ietf.org/doc/html/rfc9001#section-5.4.2
            if (fw.w.written().size() + wire.len < auth_tag) {
                fw.write(frame::PaddingFrame{});  // apply one
                fw.w.write(0, auth_tag - fw.w.written().size() - wire.len);
            }
        }

    }  // namespace fnet::quic::frame
}  // namespace futils

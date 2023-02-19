/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// writer - frame writer
#pragma once
#include "../packet/summary.h"

namespace utils {
    namespace fnet::quic::frame {
        struct fwriter {
            io::writer& w;
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
                status.apply(frame.get_type());
                return true;
            }
        };
    }  // namespace fnet::quic::frame
}  // namespace utils

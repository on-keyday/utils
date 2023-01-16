/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_frame - packet and frame utility
#pragma once
#include "packet_util.h"
// #include "../frame/vframe.h"

namespace utils {
    namespace dnet {
        namespace quic::packet {
            // returns (count,packet_len,payload_len,limit_reached)
            constexpr auto guess_packet_length_with_frames(
                size_t limit, PacketType typ, size_t dstID, size_t srcID, QPacketNumber pn, auto& frames,
                size_t padding = 0) {
                size_t frame_count = frames.size();
                FrameType prev = FrameType::PADDING;
                return packet::guess_packet_and_payload_len(
                    limit, typ, dstID, srcID, 0, pn, crypto::authentication_tag_length + padding,
                    [&](size_t i, size_t& payload_len) {
                        if (i >= frame_count) {
                            return;
                        }
                        if (is_TailFrame(prev)) {
                            return;
                        }
                        /*
                        frame::vframe_apply_lite(frames[i], [&](auto* ptr) {
                            payload_len += ptr->frame()->render_len();
                            prev = ptr->frame()->type.type_detail();
                        });*/
                    });
            }
        }  // namespace quic::packet
    }      // namespace dnet
}  // namespace utils

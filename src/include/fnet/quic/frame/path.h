/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "typeonly.h"
#include "../../../binary/number.h"

namespace futils {
    namespace fnet::quic::frame {
        using PingFrame = TypeOnly<FrameType::PING>;

        template <FrameType t>
        struct PathBase : Frame {
            std::uint64_t data;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return t;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return parse_check(r, t) &&
                       binary::read_num(r, data);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(t) + 8;
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return type_minwrite(w, t) &&
                       binary::write_num(w, data);
            }
        };

        using PathChallengeFrame = PathBase<FrameType::PATH_CHALLENGE>;
        using PathResponseFrame = PathBase<FrameType::PATH_RESPONSE>;

        namespace test {
            constexpr bool check_path() {
                PathChallengeFrame frame;
                frame.data = 0x9384933839383930;
                return do_test(frame, [&] {
                    return frame.type.type() == FrameType::PATH_CHALLENGE &&
                           frame.data == 0x9384933839383930;
                });
            }
        }  // namespace test

    }  // namespace fnet::quic::frame
}  // namespace futils

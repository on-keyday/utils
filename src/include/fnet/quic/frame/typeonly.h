/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "base.h"

namespace futils {
    namespace fnet::quic::frame {
        template <FrameType t>
        struct TypeOnly : Frame {
            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return t;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return Frame::parse(r) && type.type_detail() == t;
            }

            constexpr size_t len(bool = false) const noexcept {
                return varint::min_len(std::uint64_t(t));
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return varint::write(w, varint::Value(std::uint64_t(t)));
            }
        };

        using PaddingFrame = TypeOnly<FrameType::PADDING>;

        namespace test {
            constexpr bool check_padding() {
                PaddingFrame frame;
                return do_test(frame, [] { return true; });
            }
        }  // namespace test

    }  // namespace fnet::quic::frame
}  // namespace futils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "base.h"

namespace utils {
    namespace dnet::quic::frame {
        template <FrameType t>
        struct TypeOnly : Frame {
            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return t;
            }

            constexpr bool parse(io::reader& r) noexcept {
                return Frame::parse(r) && type.type_detail() == t;
            }

            constexpr size_t len(bool = false) const noexcept {
                return varint::min_len(std::uint64_t(t));
            }

            constexpr bool render(io::writer& w) const noexcept {
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

    }  // namespace dnet::quic::frame
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "base.h"

namespace utils {
    namespace fnet::quic::frame {
        struct NewTokenFrame : Frame {
            varint::Value token_length;  // only parse
            view::rvec token;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return FrameType::NEW_TOKEN;
            }

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, FrameType::NEW_TOKEN) &&
                       varint::read(r, token_length) &&
                       r.read(token, token_length);
            }

            constexpr size_t len(bool use_length_field = false) const noexcept {
                return type_minlen(FrameType::NEW_TOKEN) +
                       (use_length_field ? varint::len(token_length) : varint::len(varint::Value(token.size()))) +
                       token.size();
            }

            constexpr bool render(io::writer& w) const noexcept {
                return type_minwrite(w, FrameType::NEW_TOKEN) &&
                       varint::write(w, token.size()) &&
                       w.write(token);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(token);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(token);
            }
        };

        namespace test {
            constexpr bool check_new_token() {
                NewTokenFrame frame;
                byte tok[] = "9393";
                frame.token = view::rvec(tok, 4);
                return do_test(frame, [&] {
                    return frame.type.type() == FrameType::NEW_TOKEN &&
                           frame.token_length == 4 &&
                           frame.token == view::rvec(tok, 4);
                });
            }
        }  // namespace test

    }  // namespace fnet::quic::frame
}  // namespace utils

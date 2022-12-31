/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto - cryptographic frame
#pragma once
#include "typeonly.h"

namespace utils {
    namespace dnet::quic::frame {
        using HandshakeDoneFrame = TypeOnly<FrameType::HANDSHAKE_DONE>;

        struct CryptoFrame : Frame {
            varint::Value offset;
            varint::Value length;  // only parse
            view::rvec crypto_data;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return FrameType::CRYPTO;
            }

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, FrameType::CRYPTO) &&
                       varint::read(r, offset) &&
                       varint::read(r, length) &&
                       r.read(crypto_data, length);
            }

            constexpr size_t len(bool use_length_field = false) const noexcept {
                return type_minlen(FrameType::CRYPTO) +
                       varint::len(offset) +
                       (use_length_field ? varint::len(length) : varint::len(varint::Value(crypto_data.size()))) +
                       crypto_data.size();
            }

            constexpr bool render(io::writer& w) const noexcept {
                return type_minwrite(w, FrameType::CRYPTO) &&
                       varint::write(w, offset) &&
                       varint::write(w, crypto_data.size()) &&
                       w.write(crypto_data);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(crypto_data);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(crypto_data);
            }
        };

        namespace test {
            constexpr bool check_crypto() {
                byte data[] = "crypto data test";
                CryptoFrame frame;
                frame.offset = 3293;
                frame.crypto_data = view::rvec(data, 16);
                return do_test(frame, [&] {
                    return frame.offset == 3293 &&
                           frame.length == 16 &&
                           frame.crypto_data == view::rvec(data, 16);
                });
            }
        }  // namespace test

    }  // namespace dnet::quic::frame
}  // namespace utils

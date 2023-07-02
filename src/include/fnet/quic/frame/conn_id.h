/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn_id - connection id management
#pragma once
#include "base.h"

namespace utils {
    namespace fnet::quic::frame {

        struct NewConnectionIDFrame : Frame {
            varint::Value sequence_number;
            varint::Value retire_proior_to;
            byte length = 0;  // only parse
            view::rvec connectionID;
            byte stateless_reset_token[16];
            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return FrameType::NEW_CONNECTION_ID;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return parse_check(r, FrameType::NEW_CONNECTION_ID) &&
                       varint::read(r, sequence_number) &&
                       varint::read(r, retire_proior_to) &&
                       r.read(view::wvec(&length, 1)) &&
                       r.read(connectionID, length) &&
                       r.read(stateless_reset_token);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(FrameType::NEW_CONNECTION_ID) +
                       varint::len(sequence_number) +
                       varint::len(retire_proior_to) +
                       1 +
                       connectionID.size() +
                       16;
            }

            constexpr bool render(binary::writer& w) const noexcept {
                if (connectionID.size() > 0xFF) {
                    return false;
                }
                byte len = connectionID.size();
                return type_minwrite(w, FrameType::NEW_CONNECTION_ID) &&
                       varint::write(w, sequence_number) &&
                       varint::write(w, retire_proior_to) &&
                       w.write(view::rvec(&len, 1)) &&
                       w.write(connectionID) &&
                       w.write(stateless_reset_token);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(connectionID);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(connectionID);
            }
        };

        struct RetireConnectionIDFrame : Frame {
            varint::Value sequence_number;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return FrameType::RETIRE_CONNECTION_ID;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return parse_check(r, FrameType::RETIRE_CONNECTION_ID) &&
                       varint::read(r, sequence_number);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(FrameType::RETIRE_CONNECTION_ID) +
                       varint::len(sequence_number);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return type_minwrite(w, FrameType::RETIRE_CONNECTION_ID) &&
                       varint::write(w, sequence_number);
            }
        };

        namespace test {
            constexpr bool check_new_connection_id() {
                NewConnectionIDFrame frame;
                frame.sequence_number = 1;
                frame.retire_proior_to = 0;
                byte data[] = "hello or world !";
                view::copy(frame.stateless_reset_token, data);
                frame.connectionID = view::rvec(data, 16);
                return do_test(frame, [&] {
                    return frame.type.type() == FrameType::NEW_CONNECTION_ID &&
                           frame.sequence_number == 1 &&
                           frame.retire_proior_to == 0 &&
                           frame.length == 16 &&
                           frame.connectionID == view::rvec(data, 16) &&
                           frame.stateless_reset_token == view::rvec(data, 16);
                });
            }

            constexpr bool check_retire_connection_id() {
                RetireConnectionIDFrame frame;
                frame.sequence_number = 0;
                frame.sequence_number.len = 8;
                return do_test(frame, [&] {
                    return frame.type.type() == FrameType::RETIRE_CONNECTION_ID &&
                           frame.sequence_number.value == 0 &&
                           frame.sequence_number.len == 8;
                });
            }
        }  // namespace test

    }  // namespace fnet::quic::frame
}  // namespace utils

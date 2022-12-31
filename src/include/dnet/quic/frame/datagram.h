/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// datagram - DATAGRAM frame
#pragma once
#include "base.h"

namespace utils {
    namespace dnet::quic::frame {
        struct DatagramFrame : Frame {
            varint::Value length;  // only parse
            view::rvec datagram_data;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                if (on_cast) {
                    return FrameType::DATAGRAM;
                }
                return type.type_detail() == FrameType::DATAGRAM
                           ? FrameType::DATAGRAM
                           : FrameType::DATAGRAM_LEN;
            }

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, FrameType::DATAGRAM) &&
                       (type.type_detail() == FrameType::DATAGRAM_LEN
                            ? varint::read(r, length) && r.read(datagram_data, length)
                            : (datagram_data = r.read_best(~0), true));
            }

            constexpr size_t len(bool use_length_field = false) const noexcept {
                return type_minlen(FrameType::DATAGRAM) +
                       (type.type_detail() == FrameType::DATAGRAM
                            ? 0
                        : use_length_field
                            ? varint::len(length)
                            : varint::len(varint::Value(datagram_data.size()))) +
                       datagram_data.size();
            }

            constexpr bool render(io::writer& w) const noexcept {
                return type_minwrite(w, get_type()) &&
                       (type.type_detail() == FrameType::DATAGRAM ||
                        varint::write(w, datagram_data.size())) &&
                       w.write(datagram_data);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(datagram_data);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(datagram_data);
            }
        };

        namespace test {
            constexpr bool check_dgram() {
                DatagramFrame frame;
                byte buf[] = "test";
                frame.datagram_data = view::rvec(buf, 4);
                bool ok1 = do_test(frame, [&] {
                    return frame.type.type_detail() == FrameType::DATAGRAM_LEN &&
                           frame.length == frame.datagram_data.size() &&
                           frame.datagram_data == view::rvec(buf, 4);
                });
                frame.datagram_data = view::rvec(buf, 3);
                frame.type = FrameType::DATAGRAM;
                bool ok2 = do_test(frame, [&] {
                    return frame.type.type_detail() == FrameType::DATAGRAM &&
                           frame.datagram_data == view::rvec(buf, 3);
                });
                return ok1 && ok2;
            }

        }  // namespace test
    }      // namespace dnet::quic::frame
}  // namespace utils

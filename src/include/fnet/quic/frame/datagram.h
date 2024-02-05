/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// datagram - DATAGRAM frame
#pragma once
#include "base.h"
#include "calc_data.h"

namespace futils {
    namespace fnet::quic::frame {
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

            constexpr bool parse(binary::reader& r) noexcept {
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

            constexpr bool render(binary::writer& w) const noexcept {
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

        constexpr std::pair<DatagramFrame, bool> make_fit_size_Datagram(std::uint64_t to_fit, view::rvec src) {
            DatagramFrame dgram;
            dgram.type = FrameType::DATAGRAM;
            dgram.datagram_data = src;
            const auto no_len = dgram.len();
            if (no_len == to_fit) {
                return {dgram, true};
            }
            dgram.type = FrameType::DATAGRAM_LEN;
            const auto with_len = dgram.len();
            if (with_len <= to_fit) {
                return {dgram, true};
            }
            return {{}, false};
        }

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

            constexpr bool check_make_dgram() {
                byte buf[] = "test";
                auto [dgram, ok] = make_fit_size_Datagram(3, view::rvec(buf, 4));
                if (ok) {
                    return false;
                }
                std::tie(dgram, ok) = make_fit_size_Datagram(5, view::rvec(buf, 4));
                if (!ok) {
                    return false;
                }
                if (dgram.type.type_detail() != FrameType::DATAGRAM) {
                    return false;
                }
                std::tie(dgram, ok) = make_fit_size_Datagram(6, view::rvec(buf, 4));
                if (!ok) {
                    return false;
                }
                if (dgram.type.type_detail() != FrameType::DATAGRAM_LEN) {
                    return false;
                }
                std::tie(dgram, ok) = make_fit_size_Datagram(6, view::rvec(buf, 64));
                if (ok) {
                    return false;
                }
                std::tie(dgram, ok) = make_fit_size_Datagram(66, view::rvec(buf, 64));
                if (ok) {
                    return false;
                }
                std::tie(dgram, ok) = make_fit_size_Datagram(67, view::rvec(buf, 64));
                if (!ok) {
                    return false;
                }
                if (dgram.type.type_detail() != FrameType::DATAGRAM_LEN) {
                    return false;
                }
                return true;
            }

            static_assert(check_make_dgram());

        }  // namespace test
    }      // namespace fnet::quic::frame
}  // namespace futils

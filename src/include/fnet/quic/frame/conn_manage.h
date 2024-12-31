/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn_manage - connection management
#pragma once
#include "base.h"

namespace futils {
    namespace fnet::quic::frame {

        template <FrameType t>
        struct MaxDataBase : Frame {
            varint::Value max_data;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return t;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return parse_check(r, t) &&
                       varint::read(r, max_data);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(t) +
                       varint::len(max_data);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return type_minwrite(w, t) &&
                       varint::write(w, max_data);
            }
        };

        using MaxDataFrame = MaxDataBase<FrameType::MAX_DATA>;
        using DataBlockedFrame = MaxDataBase<FrameType::DATA_BLOCKED>;

        template <FrameType t>
        struct MaxStreamsBase : Frame {
            varint::Value max_streams;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                if (on_cast) {
                    return t;
                }
                return type.type_detail();  // not check anyway
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return parse_check(r, t) &&
                       varint::read(r, max_streams);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(t) +
                       varint::len(max_streams);
            }

            constexpr bool render(binary::writer& w) const noexcept {
                if (type.type() != t) {
                    return false;
                }
                return type_minwrite(w, type) &&
                       varint::write(w, max_streams);
            }
        };

        using MaxStreamsFrame = MaxStreamsBase<FrameType::MAX_STREAMS>;
        using StreamsBlockedFrame = MaxStreamsBase<FrameType::STREAMS_BLOCKED>;

        struct ConnectionCloseFrame : Frame {
            varint::Value error_code;
            varint::Value frame_type;
            varint::Value reason_phrase_length;  // only parse
            view::rvec reason_phrase;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                if (on_cast) {
                    return FrameType::CONNECTION_CLOSE;
                }
                return type.type_detail() == FrameType::CONNECTION_CLOSE_APP
                           ? FrameType::CONNECTION_CLOSE_APP
                           : FrameType::CONNECTION_CLOSE;
            }

            constexpr bool parse(binary::reader& r) noexcept {
                return parse_check(r, FrameType::CONNECTION_CLOSE) &&
                       varint::read(r, error_code) &&
                       (type.type_detail() == FrameType::CONNECTION_CLOSE_APP ||
                        varint::read(r, frame_type)) &&
                       varint::read(r, reason_phrase_length) &&
                       r.read(reason_phrase, reason_phrase_length);
            }

            constexpr size_t len(bool use_length_field = false) const noexcept {
                return type_minlen(FrameType::CONNECTION_CLOSE) +
                       varint::len(error_code) +
                       (type.type_detail() != FrameType::CONNECTION_CLOSE_APP
                            ? varint::len(frame_type)
                            : 0) +
                       (use_length_field
                            ? varint::len(reason_phrase_length)
                            : varint::len(varint::Value(reason_phrase.size()))) +
                       reason_phrase.size();
            }

            constexpr bool render(binary::writer& w) const noexcept {
                return type_minwrite(w, get_type()) &&
                       varint::write(w, error_code) &&
                       (type.type_detail() == FrameType::CONNECTION_CLOSE_APP ||
                        varint::write(w, frame_type)) &&
                       varint::write(w, reason_phrase.size()) &&
                       w.write(reason_phrase);
            }

            static constexpr size_t rvec_field_count() noexcept {
                return 1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(reason_phrase);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(reason_phrase);
            }
        };

        namespace test {

            constexpr bool check_max_data() {
                MaxDataFrame frame;
                frame.max_data = 39392;
                return do_test(frame, [&] {
                    return frame.type.type() == FrameType::MAX_DATA &&
                           frame.max_data.value == 39392 &&
                           frame.max_data.len == 4;
                });
            }

            constexpr bool check_max_streams() {
                MaxStreamsFrame frame;
                frame.max_streams = 102;
                if (do_test(frame, [] { return true; })) {
                    return false;
                }
                frame.type = FrameType::MAX_STREAMS_BIDI;
                bool ok = do_test(frame, [&] {
                    return frame.type.type() == FrameType::MAX_STREAMS_BIDI &&
                           frame.max_streams == 102;
                });
                if (!ok) {
                    return false;
                }
                frame.type = FrameType::MAX_STREAMS_UNI;
                return do_test(frame, [&] {
                    return frame.type.type_detail() == FrameType::MAX_STREAMS_UNI &&
                           frame.max_streams == 102;
                });
            }

            constexpr bool check_connection_close() {
                ConnectionCloseFrame frame;
                frame.error_code = 2993;
                frame.frame_type = 292;
                byte data[] = "this is test";
                frame.reason_phrase = view::rvec(data, sizeof(data) - 1);
                bool ok = do_test(frame, [&] {
                    return frame.type.type_detail() == FrameType::CONNECTION_CLOSE &&
                           frame.error_code == 2993 &&
                           frame.frame_type == 292 &&
                           frame.frame_type.len == 2 &&
                           frame.reason_phrase_length == sizeof(data) - 1 &&
                           frame.reason_phrase == view::rvec(data, sizeof(data) - 1);
                });
                if (!ok) {
                    return false;
                }
                frame.type = FrameType::CONNECTION_CLOSE_APP;
                frame.frame_type = 0;
                frame.reason_phrase = view::rvec(data, sizeof(data) - 1);
                return do_test(frame, [&] {
                    return frame.type.type_detail() == FrameType::CONNECTION_CLOSE_APP &&
                           frame.error_code == 2993 &&
                           frame.frame_type.value == 0 &&
                           frame.frame_type.value == 0 &&
                           frame.reason_phrase_length == sizeof(data) - 1 &&
                           frame.reason_phrase == view::rvec(data, sizeof(data) - 1);
                });
            }
        }  // namespace test

    }  // namespace fnet::quic::frame
}  // namespace futils

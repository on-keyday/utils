/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream_manage - stream management frame
#pragma once
#include "base.h"

namespace utils {
    namespace dnet::quic::frame {
        struct ResetStreamFrame : Frame {
            varint::Value streamID;
            varint::Value application_protocol_error_code;
            varint::Value final_size;
            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return FrameType::RESET_STREAM;
            }

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, FrameType::RESET_STREAM) &&
                       varint::read(r, streamID) &&
                       varint::read(r, application_protocol_error_code) &&
                       varint::read(r, final_size);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(FrameType::RESET_STREAM) +
                       varint::len(streamID) +
                       varint::len(application_protocol_error_code) +
                       varint::len(final_size);
            }

            constexpr bool render(io::writer& w) const noexcept {
                return type_minwrite(w, FrameType::RESET_STREAM) &&
                       varint::write(w, streamID) &&
                       varint::write(w, application_protocol_error_code) &&
                       varint::write(w, final_size);
            }
        };

        struct StopSendingFrame : Frame {
            varint::Value streamID;
            varint::Value application_protocol_error_code;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return FrameType::STOP_SENDING;
            }

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, FrameType::STOP_SENDING) &&
                       varint::read(r, streamID) &&
                       varint::read(r, application_protocol_error_code);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(FrameType::STOP_SENDING) +
                       varint::len(streamID) +
                       varint::len(application_protocol_error_code);
            }

            constexpr bool render(io::writer& w) const noexcept {
                return type_minwrite(w, FrameType::STOP_SENDING) &&
                       varint::write(w, streamID) &&
                       varint::write(w, application_protocol_error_code);
            }
        };

        struct StreamFrame : Frame {
            varint::Value streamID;
            varint::Value offset;
            varint::Value length;  // on parse
            view::rvec stream_data;
            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                if (on_cast) {
                    return FrameType::STREAM;
                }
                if (type.type() != FrameType::STREAM) {
                    return make_STREAM(offset.value != 0, true, false);
                }
                return type.type_detail();
            }

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, FrameType::STREAM) &&
                       varint::read(r, streamID) &&
                       (type.STREAM_off()
                            ? varint::read(r, offset)
                            : (offset = 0, true)) &&
                       (!type.STREAM_len() || varint::read(r, length)) &&
                       (type.STREAM_len()
                            ? r.read(stream_data, length)
                            : (stream_data = r.read_best(~0), true));
            }

            constexpr size_t len(bool use_length_field = false) const noexcept {
                auto tyval = FrameFlags{get_type()};
                return type_minlen(FrameType::STREAM) +
                       varint::len(streamID) +
                       (tyval.STREAM_off() ? varint::len(offset) : 0) +
                       (!tyval.STREAM_len()
                            ? 0
                        : use_length_field
                            ? varint::len(length)
                            : varint::len(varint::Value(stream_data.size()))) +
                       stream_data.size();
            }

            constexpr bool render(io::writer& w) const noexcept {
                auto tyval = FrameFlags{get_type()};
                return type_minwrite(w, tyval) &&
                       varint::write(w, streamID) &&
                       (!tyval.STREAM_off() || varint::write(w, offset)) &&
                       (!tyval.STREAM_len() || varint::write(w, stream_data.size())) &&
                       w.write(stream_data);
            }

            constexpr static size_t rvec_field_count() noexcept {
                return 1;
            }

            constexpr void visit_rvec(auto&& cb) noexcept {
                cb(stream_data);
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {
                cb(stream_data);
            }
        };

        template <FrameType t>
        struct MaxStreamDataBase : Frame {
            varint::Value streamID;
            varint::Value max_stream_data;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                return t;
            }

            constexpr bool parse(io::reader& r) noexcept {
                return parse_check(r, t) &&
                       varint::read(r, streamID) &&
                       varint::read(r, max_stream_data);
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(t) +
                       varint::len(streamID) +
                       varint::len(max_stream_data);
            }

            constexpr bool render(io::writer& w) const noexcept {
                return type_minwrite(w, t) &&
                       varint::write(w, streamID) &&
                       varint::write(w, max_stream_data);
            }
        };

        using StreamDataBlockedFrame = MaxStreamDataBase<FrameType::STREAM_DATA_BLOCKED>;
        using MaxStreamDataFrame = MaxStreamDataBase<FrameType::MAX_STREAM_DATA>;

        namespace test {
            constexpr bool check_reset_stream() {
                ResetStreamFrame frame;
                frame.streamID = 2030;
                frame.application_protocol_error_code = 0x40000000;
                frame.final_size = 294928833;
                return do_test(frame, [&] {
                    return frame.streamID.value == 2030 &&
                           frame.streamID.len == 2 &&
                           frame.application_protocol_error_code.len == 8 &&
                           frame.application_protocol_error_code.value == 0x40000000 &&
                           frame.final_size.len == 4 &&
                           frame.final_size.value == 294928833;
                });
            }

            constexpr bool check_stop_sending() {
                StopSendingFrame frame;
                frame.application_protocol_error_code = 0x3F;
                frame.streamID = 0x3FFF;
                return do_test(frame, [&] {
                    return frame.application_protocol_error_code.len == 1 &&
                           frame.application_protocol_error_code.value == 0x3f &&
                           frame.streamID.len == 2 &&
                           frame.streamID.value == 0x3fff;
                });
            }

            constexpr bool check_stream() {
                StreamFrame frame;
                byte data[] = {0x1, 0x00, 0x34, 0x32, 0x33};
                frame.stream_data = data;
                frame.offset = 1;
                bool ok1 = do_test(frame, [&] {
                    return frame.type.type_detail() == make_STREAM(true, true, false) &&
                           frame.streamID.len == 1 &&
                           frame.streamID.value == 0 &&
                           frame.offset.len == 1 &&
                           frame.offset.value == 1 &&
                           frame.length.len == 1 &&
                           frame.length.value == 5 &&
                           frame.stream_data == data;
                });
                if (!ok1) {
                    return false;
                }
                frame.stream_data = data;
                frame.offset = 0;
                frame.type = make_STREAM(false, false, true);  // reset
                bool ok2 = do_test(frame, [&] {
                    return frame.type.type_detail() == make_STREAM(false, false, true) &&
                           frame.offset.value == 0 &&
                           frame.offset.len == 0 &&
                           frame.stream_data == data;
                });
                if (!ok2) {
                    return false;
                }
                frame.stream_data = data;
                frame.offset = 283839;
                frame.type = make_STREAM(true, false, false);
                bool ok3 = do_test(frame, [&] {
                    return frame.type.type_detail() == make_STREAM(true, false, false) &&
                           frame.offset.value == 283839 &&
                           frame.offset.len == 4 &&
                           frame.stream_data == data;
                });
                if (!ok3) {
                    return false;
                }
                return true;
            }

            constexpr bool check_max_stream_data() {
                MaxStreamDataFrame frame;
                frame.max_stream_data = 100;
                frame.max_stream_data.len = 1;
                if (do_test(frame, [] { return true; })) {
                    return false;  // should fail !
                }
                frame.max_stream_data.len = 4;
                return do_test(frame, [&] {
                    return frame.type.type() == FrameType::MAX_STREAM_DATA &&
                           frame.max_stream_data.value == 100 &&
                           frame.max_stream_data.len == 4;
                });
            }
        }  // namespace test

    }  // namespace dnet::quic::frame
}  // namespace utils

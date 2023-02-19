/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream_manage - stream management frame
#pragma once
#include "base.h"
#include "calc_data.h"

namespace utils {
    namespace fnet::quic::frame {
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

        constexpr std::uint64_t calc_stream_overhead(std::uint64_t stream_id, std::uint64_t offset, bool has_ofs) noexcept {
            return 1 + varint::len(stream_id) + (has_ofs ? varint::len(offset) : 0);
        }

        constexpr StreamDataRange stream_size_range(std::uint64_t stream_id, std::uint64_t offset, std::uint64_t max_length, bool allow_zero) noexcept {
            StreamDataRange range;
            // Fields below:
            // Type
            // Stream ID
            // Offset
            range.fixed_length = calc_stream_overhead(stream_id, offset, offset != 0);
            // Length Field
            range.max_length = max_length;
            range.min_zero = allow_zero;
            return range;
        }

        // make_fit_size_Stream makes StreamFrame which size is fit to to_fit.
        // 1. try to calculate size of StreamFrame which length field omitted.
        //    if calculated frame size is strictly fit to to_fit,then create StreamFrame.
        // 2. try to calculate size of StreamFrame which length field exists.
        //    if calculated frame size is equal to or smaller than to_fit, then create StreamFrame.
        // 3. if above two way is failed, returns ({},false) to report error
        // fin flag will set if fin is true and calculated stream data length is equal to src.size()
        constexpr std::pair<StreamFrame, bool> make_fit_size_Stream(size_t to_fit, size_t stream_id, size_t offset, view::rvec src, bool fin, bool allow_zero) {
            auto range = stream_size_range(stream_id, offset, src.size(), allow_zero);
            auto [ok, no_len_data, valid] = shrink_to_fit_no_len(to_fit, range);
            if (valid) {
                StreamFrame frame;
                frame.type = make_STREAM(offset != 0, false, fin && src.size() == no_len_data);
                frame.streamID = stream_id;
                frame.offset = offset;
                frame.length = no_len_data;
                frame.stream_data = src.substr(0, no_len_data);
                return {frame, true};
            }
            // stream frame has least 2 byte,
            // so if ok this is 0, this means no longer cannot write anything
            if (ok == 0) {
                return {{}, false};
            }
            auto [_, with_len_data, use_len] = shrink_to_fit_with_len(to_fit, range);
            if (use_len) {
                StreamFrame frame;
                frame.type = make_STREAM(offset != 0, true, fin && src.size() == with_len_data);
                frame.streamID = stream_id;
                frame.offset = offset;
                frame.length = with_len_data;
                frame.stream_data = src.substr(0, with_len_data);
                return {frame, true};
            }
            return {{}, false};
        }

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

        // get_streamID gets streamID if f is stream related frame
        // stream related frames are
        // - STOP_SENDING
        // - RESET_STREAM
        // - STREAM
        // - STREAM_DATA_BLOCKED
        // - MAX_STREAM_DATA
        constexpr std::pair<std::uint64_t, bool> get_streamID(const frame::Frame& f) noexcept {
            switch (f.type.type()) {
                case FrameType::STOP_SENDING:
                    return {static_cast<const frame::StopSendingFrame&>(f).streamID, true};
                case FrameType::RESET_STREAM:
                    return {static_cast<const frame::ResetStreamFrame&>(f).streamID, true};
                case FrameType::STREAM:
                    return {static_cast<const frame::StreamFrame&>(f).streamID, true};
                case FrameType::STREAM_DATA_BLOCKED:
                    return {static_cast<const frame::StreamDataBlockedFrame&>(f).streamID, true};
                case FrameType::MAX_STREAM_DATA:
                    return {static_cast<const frame::MaxStreamDataFrame&>(f).streamID, true};
                default:
                    return {~std::uint64_t(0), false};
            }
        }

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

            constexpr auto check_stream_range() {
                constexpr auto range = stream_size_range(1, 0, 7994, true);
                bool ok = range.length_exists_max_len() == 1 + 1 + 0 + 2 + 7994 &&
                          range.length_no_max_len() == 1 + 1 + 0 + 0 + 7994 &&
                          range.length_exists_min_len() == 1 + 1 + 0 + 1 + 0 &&
                          range.length_no_min_len() == 1 + 1 + 0 + 0 + 0;
                if (!ok) {
                    return false;
                }
                auto check = [&](size_t fit, size_t om_flen, size_t om_data, bool om_val,
                                 size_t ex_flen, size_t ex_dat, bool ex_val) {
                    frame::StreamFrame frame;
                    auto [omit_flen, omit_data, omit_valid] = shrink_to_fit_no_len(fit, range);
                    ok = omit_flen == om_flen && omit_data == om_data && omit_valid == om_val;
                    if (!ok) {
                        return false;
                    }
                    if (omit_valid) {
                        frame.type = make_STREAM(false, false, false);
                        frame.length = omit_data;
                        if (frame.len(true) + omit_data != omit_flen) {
                            return false;
                        }
                    }
                    auto [exist_flen, exist_data, exist_valid] = shrink_to_fit_with_len(fit, range);
                    ok = exist_flen == ex_flen && exist_data == ex_dat && exist_valid == ex_val;
                    if (!ok) {
                        return false;
                    }
                    if (exist_valid) {
                        frame.type = make_STREAM(false, true, false);
                        frame.length = exist_data;
                        if (frame.len(true) + exist_data != exist_flen) {
                            return false;
                        }
                    }
                    return true;
                };
                if (!check(3,
                           3, 1, true,
                           3, 0, true)) {
                    return false;
                }
                if (!check(493,
                           493, 491, true,
                           493, 489, true)) {
                    return false;
                }
                if (!check(7998,
                           7996, 7994, false,
                           7998, 7994, true)) {
                    return false;
                }
                if (!check(67,
                           67, 65, true,
                           66, 63, true)) {
                    return false;
                }
                return true;
            }

            static_assert(check_stream_range());

        }  // namespace test

    }  // namespace fnet::quic::frame
}  // namespace utils

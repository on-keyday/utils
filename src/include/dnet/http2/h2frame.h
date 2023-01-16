/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2frame - http2 frame
#pragma once
#include "../dll/dllh.h"
#include <wrap/light/enum.h>
#include <cstddef>
#include "../../io/number.h"

namespace utils {
    namespace dnet::http2 {
        enum class H2Error {
            none = 0x0,
            protocol = 0x1,
            internal = 0x2,
            flow_control = 0x3,
            settings_timeout = 0x4,
            stream_closed = 0x5,
            frame_size = 0x6,
            refused = 0x7,
            cancel = 0x8,
            compression = 0x9,

            connect = 0xa,
            enhance_your_clam = 0xb,
            inadequate_security = 0xc,
            http_1_1_required = 0xd,

            user_defined_bit = 0x100,
            unknown,
            read_len,
            read_type,
            read_flag,
            read_id,
            read_padding,
            read_data,
            read_depend,
            read_weight,
            read_code,
            read_increment,
            read_processed_id,

            type_mismatch,
            transport,
            ping_failed,
        };

        DEFINE_ENUM_FLAGOP(H2Error)

        BEGIN_ENUM_STRING_MSG(H2Error, error_msg)
        ENUM_STRING_MSG(H2Error::none, "no error")
        ENUM_STRING_MSG(H2Error::protocol, "protocol error")
        ENUM_STRING_MSG(H2Error::internal, "internal error")
        ENUM_STRING_MSG(H2Error::flow_control, "flow control error")
        ENUM_STRING_MSG(H2Error::settings_timeout, "settings timeout")
        ENUM_STRING_MSG(H2Error::stream_closed, "stream closed")
        ENUM_STRING_MSG(H2Error::frame_size, "frame size error")
        ENUM_STRING_MSG(H2Error::refused, "stream refused")
        ENUM_STRING_MSG(H2Error::cancel, "canceled")
        ENUM_STRING_MSG(H2Error::compression, "compression error")
        ENUM_STRING_MSG(H2Error::connect, "connect error")
        ENUM_STRING_MSG(H2Error::enhance_your_clam, "enchance your clam")
        ENUM_STRING_MSG(H2Error::inadequate_security, "inadequate security")
        ENUM_STRING_MSG(H2Error::http_1_1_required, "http/1.1 required")
        ENUM_STRING_MSG(H2Error::transport, "transport layer error")
        END_ENUM_STRING_MSG("unknown or internal error")

        struct ErrCode {
            // library error code
            int err = 0;
            // http2 error code
            H2Error h2err = H2Error::none;
            // stream level error
            bool strm = false;
            // debug data
            const char* debug = nullptr;
        };

        constexpr auto http2_connection_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

        namespace frame {

            enum class FrameType : std::uint8_t {
                data = 0x0,
                header = 0x1,
                priority = 0x2,
                rst_stream = 0x3,
                settings = 0x4,
                push_promise = 0x5,
                ping = 0x6,
                goaway = 0x7,
                window_update = 0x8,
                continuous = 0x9,
                invalid = 0xff,
            };

            BEGIN_ENUM_STRING_MSG(FrameType, frame_name)
            ENUM_STRING_MSG(FrameType::data, "DATA")
            ENUM_STRING_MSG(FrameType::header, "HEADER")
            ENUM_STRING_MSG(FrameType::priority, "PRIORITY")
            ENUM_STRING_MSG(FrameType::rst_stream, "RST_STREAM")
            ENUM_STRING_MSG(FrameType::settings, "SETTINGS")
            ENUM_STRING_MSG(FrameType::push_promise, "PUSH_PROMISE")
            ENUM_STRING_MSG(FrameType::ping, "PING")
            ENUM_STRING_MSG(FrameType::goaway, "GOAWAY")
            ENUM_STRING_MSG(FrameType::window_update, "WINDOW_UPDATE")
            ENUM_STRING_MSG(FrameType::continuous, "CONTINUOUS")
            END_ENUM_STRING_MSG("UNKNOWN")

            enum Flag : std::uint8_t {
                none = 0x0,
                ack = 0x1,
                end_stream = 0x1,
                end_headers = 0x4,
                padded = 0x8,
                priority = 0x20,
            };

            DEFINE_ENUM_FLAGOP(Flag)

            template <class Buf>
            Buf flag_state(Flag flag, bool ack) {
                Buf buf;
                bool first = true;
                auto write = [&](auto&& v) {
                    if (!first) {
                        helper::append(buf, " | ");
                    }
                    first = false;
                    helper::append(buf, v);
                };
                if (flag & Flag::ack) {
                    if (ack) {
                        write("ack");
                    }
                    else {
                        write("end_stream");
                    }
                }
                if (flag & Flag::end_headers) {
                    write("end_headers");
                }
                if (flag & Flag::padded) {
                    write("padded");
                }
                if (flag & Flag::priority) {
                    write("priority");
                }
                if (first) {
                    write("none");
                }
                return buf;
            }

            enum class FrameIDMode {
                conn,
                stream,
                both,
            };

            struct Frame {
                std::uint32_t len = 0;
                const FrameType type = FrameType::invalid;
                Flag flag = Flag::none;
                std::int32_t id = 0;

                constexpr Frame() = default;

               protected:
                constexpr Frame(FrameType type)
                    : type(type) {}

                constexpr bool render_with_len(io::writer& w, std::uint32_t id, std::uint32_t len) const noexcept {
                    return io::write_uint24(w, len) &&
                           w.write(byte(type), 1) &&
                           w.write(byte(flag), 1) &&
                           io::write_num(w, id);
                }

                constexpr std::pair<io::reader, bool> parse_and_subreader(io::reader& r, FrameIDMode mode) noexcept {
                    if (!parse(r)) {
                        return {{view::rvec{}}, false};
                    }
                    switch (mode) {
                        case FrameIDMode::conn: {
                            if (id != 0) {
                                return {view::rvec{}, false};
                            }
                        } break;
                        case FrameIDMode::stream: {
                            if (id == 0) {
                                return {view::rvec{}, false};
                            }
                        } break;
                        default:
                            break;
                    }
                    auto [data, ok] = r.read(len);
                    if (!ok) {
                        return {view::rvec{}, false};
                    }
                    return {{data}, true};
                }

               public:
                constexpr bool parse(io::reader& r) noexcept {
                    byte tmp = 0;
                    return io::read_uint24(r, len) &&
                           r.read(view::wvec(&tmp, 1)) &&
                           FrameType(tmp) == type &&
                           r.read(view::wvec(&tmp, 1)) &&
                           (flag = Flag(tmp), true) &&
                           io::read_num(r, id);
                }

                constexpr bool render(io::writer& w) const noexcept {
                    return render_with_len(w, id, len);
                }
            };

            struct Priority {
                std::uint32_t depend = 0;
                std::uint8_t weight = 0;

                constexpr bool parse(io::reader& r) noexcept {
                    return io::read_num(r, depend) &&
                           io::read_num(r, weight);
                }

                constexpr bool render(io::writer& w) const noexcept {
                    return io::write_num(w, depend) &&
                           w.write(weight, 1);
                }
            };

            struct DataFrame : Frame {
                std::uint8_t padding = 0;
                view::rvec data;
                view::rvec pad;  // parse only
                constexpr DataFrame()
                    : Frame(FrameType::data) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::stream);
                    if (!ok) {
                        return false;
                    }
                    if (flag & Flag::padded) {
                        if (!io::read_num(sub, padding)) {
                            return false;
                        }
                    }
                    else {
                        padding = 0;
                    }
                    return sub.read(data, len - padding) &&
                           sub.read(pad, padding) &&
                           sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return data.size() + (flag & Flag::padded ? 1 + padding : 0);
                }

                constexpr bool render(io::writer& w) const noexcept {
                    const auto len = data_len();
                    if (id == 0 || len > 0xffffff) {
                        return false;
                    }
                    return render_with_len(w, id, len) &&
                           (flag & Flag::padded ? w.write(padding, 1) : true) &&
                           w.write(data) &&
                           w.write(0, padding);
                }
            };

            struct HeaderFrame : Frame {
                std::uint8_t padding = 0;
                Priority priority;
                view::rvec data;
                view::rvec pad;  // parse only
                constexpr HeaderFrame()
                    : Frame(FrameType::header) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::stream);
                    if (!ok) {
                        return false;
                    }
                    if (flag & Flag::padded) {
                        if (!io::read_num(sub, padding)) {
                            return false;
                        }
                    }
                    else {
                        padding = 0;
                    }
                    if (flag & Flag::priority) {
                        if (!priority.parse(sub)) {
                            return false;
                        }
                    }
                    return sub.read(data, len - padding) &&
                           sub.read(pad, padding) &&
                           sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return data.size() + (flag & Flag::padded ? 1 + padding : 0) + (flag & Flag::priority ? 5 : 0);
                }

                constexpr bool render(io::writer& w) const noexcept {
                    const auto len = data_len();
                    if (id == 0 && len > 0xffffff) {
                        return false;
                    }
                    return render_with_len(w, id, len) &&
                           (flag & Flag::padded ? w.write(padding, 1) : true) &&
                           (flag & Flag::priority ? priority.render(w) : true) &&
                           w.write(data) &&
                           (flag & Flag::padded ? w.write(0, padding) : true);
                }
            };

            struct PriorityFrame : Frame {
                Priority priority;
                constexpr PriorityFrame()
                    : Frame(FrameType::priority) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::stream);
                    if (!ok) {
                        return false;
                    }
                    return priority.parse(sub) && sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return 5;
                }

                constexpr bool render(io::writer& w) const noexcept {
                    if (id == 0) {
                        return false;
                    }
                    return Frame::render_with_len(w, id, 5) && priority.render(w);
                }
            };

            struct RstStreamFrame : Frame {
                std::uint32_t code = 0;
                constexpr RstStreamFrame()
                    : Frame(FrameType::rst_stream) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::stream);
                    if (!ok) {
                        return false;
                    }
                    return io::read_num(sub, code) && sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return 4;
                }

                constexpr bool render(io::writer& w) const noexcept {
                    if (id == 0) {
                        return false;
                    }
                    return Frame::render_with_len(w, id, 4) && io::write_num(w, code);
                }
            };

            struct SettingsFrame : Frame {
                view::rvec settings;
                constexpr SettingsFrame()
                    : Frame(FrameType::settings) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::conn);
                    if (!ok) {
                        return false;
                    }
                    if (sub.remain().size() % 6 != 0) {
                        return false;
                    }
                    settings = sub.remain();
                    return true;
                }

                constexpr size_t data_len() const noexcept {
                    return settings.size();
                }

                constexpr bool render(io::writer& w) const noexcept {
                    if (settings.size() % 6 != 0) {
                        return false;
                    }
                    return Frame::render_with_len(w, 0, settings.size()) &&
                           w.write(settings);
                }
            };

            struct PushPromiseFrame : Frame {
                std::uint8_t padding = 0;
                std::int32_t promise = 0;
                view::rvec data;
                view::rvec pad;
                constexpr PushPromiseFrame()
                    : Frame(FrameType::push_promise) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::stream);
                    if (!ok) {
                        return false;
                    }
                    if (flag & Flag::padded) {
                        if (!io::read_num(sub, padding)) {
                            return false;
                        }
                    }
                    else {
                        padding = 0;
                    }
                    return io::read_num(r, promise) &&
                           sub.read(data, len - padding) &&
                           sub.read(pad, padding) &&
                           sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return data.size() + (flag & Flag::padded ? 1 + padding : 0) + 4;
                }

                constexpr bool render(io::writer& w) const noexcept {
                    const auto len = data_len();
                    if (id == 0 && len > 0xffffff) {
                        return false;
                    }
                    return render_with_len(w, id, len) &&
                           (flag & Flag::padded ? w.write(padding, 1) : true) &&
                           io::write_num(w, promise) &&
                           w.write(data) &&
                           (flag & Flag::padded ? w.write(0, padding) : true);
                }
            };

            struct PingFrame : Frame {
                std::uint64_t opaque = 0;
                constexpr PingFrame()
                    : Frame(FrameType::ping) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::conn);
                    if (!ok) {
                        return false;
                    }
                    return io::read_num(sub, opaque) && sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return 8;
                }

                constexpr bool render(io::writer& w) const noexcept {
                    return Frame::render_with_len(w, 0, 8) && io::write_num(w, opaque);
                }
            };

            struct GoAwayFrame : Frame {
                std::uint32_t processed_id = 0;
                std::uint32_t code = 0;
                view::rvec debug;
                constexpr GoAwayFrame()
                    : Frame(FrameType::goaway) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::conn);
                    if (!ok) {
                        return false;
                    }
                    return io::read_num(sub, processed_id) &&
                           io::read_num(sub, code) &&
                           sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return 8 + debug.size();
                }

                constexpr bool render(io::writer& w) const noexcept {
                    const auto len = data_len();
                    if (len > 0xffffff) {
                        return false;
                    }
                    return Frame::render_with_len(w, 0, len) &&
                           io::write_num(w, processed_id) &&
                           io::write_num(w, code) &&
                           w.write(debug);
                }
            };

            struct WindowUpdateFrame : Frame {
                std::int32_t increment = 0;
                constexpr WindowUpdateFrame()
                    : Frame(FrameType::window_update) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::both);
                    if (!ok) {
                        return false;
                    }
                    return io::read_num(sub, increment) && sub.empty();
                }

                constexpr size_t data_len() const noexcept {
                    return 4;
                }

                constexpr bool render(io::writer& w) const noexcept {
                    return Frame::render_with_len(w, id, 4) && io::write_num(w, increment);
                }
            };

            struct ContinuationFrame : Frame {
                view::rvec data;
                constexpr ContinuationFrame()
                    : Frame(FrameType::continuous) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::stream);
                    if (!ok) {
                        return false;
                    }
                    data = sub.remain();
                    return true;
                }

                constexpr size_t data_len() const noexcept {
                    return data.size();
                }

                constexpr bool render(io::writer& w) const noexcept {
                    if (id == 0 || data.size() > 0xffffff) {
                        return false;
                    }
                    return Frame::render_with_len(w, id, data.size()) &&
                           w.write(data);
                }
            };

            struct UnknownFrame : Frame {
                view::rvec data;
                constexpr UnknownFrame(FrameType type)
                    : Frame(type) {}

                constexpr bool parse(io::reader& r) noexcept {
                    auto [sub, ok] = parse_and_subreader(r, FrameIDMode::both);
                    if (!ok) {
                        return false;
                    }
                    data = sub.remain();
                    return true;
                }

                constexpr size_t data_len() const noexcept {
                    return data.size();
                }

                constexpr bool render(io::writer& w) const noexcept {
                    if (data.size() > 0xffffff) {
                        return false;
                    }
                    return Frame::render_with_len(w, id, data.size()) &&
                           w.write(data);
                }
            };

            template <class F>
            constexpr auto frame_typeof = []() { return F{}.type; }();

            /*
            struct ErrCode {
                // library error code
                int err = 0;
                // http2 error code
                H2Error h2err = H2Error::none;
                // stream level error
                bool strm = false;
                // debug data
                const char* debug = nullptr;
            };


            using H2Callback = bool (*)(void* user, frame::Frame* frame, ErrCode err);
            // parse_frame parses http2 frames
            // this function parses frames and detects errors if exists
            // this function would not detect error related with Stream State
            // cb would be called when a frame is parsed or frame error occuerd
            // cb would not be called when text length is not enough
            // if succeeded returns true otherwise returns false
            // and error information is set on err
            dnet_dll_export(bool) parse_frame(const char* text, size_t size, size_t& red, ErrCode& err, H2Callback cb, void* c);
            // render_frame renders a http2 frame
            // this function renders frames and detects errors if exists
            // this function would not detect error related with Stream State
            // if auto_correct is true this function corrects frame as it can (both text and frame)
            // As a point of caution, auto_correct drops padding if format is invalid.
            // if succeeded returns true otherwise returns false
            // and error information is set on err
            dnet_dll_export(bool) render_frame(char* text, size_t& size, size_t cap, ErrCode& err, frame::Frame* frame, bool auto_correct);
            // pass_frame pass a http2 frame
            // this function reads frame header and skips bytes of length
            // this fucntion provide any checks
            // if skip succeedes returns true otherwise returns false
            dnet_dll_export(bool) pass_frame(const char* text, size_t size, size_t& red);*/

            constexpr bool parse_frame(io::reader& r, auto&& cb) {
                if (r.remain().size() < 9) {
                    return false;
                }
                auto peek = r.peeker();
                std::uint32_t len = 0;
                io::read_uint24(peek, len);
                if (r.remain().size() < 9 + len) {
                    return false;
                }
                const auto type = FrameType(peek.top());
                const auto base = r.offset();
                peek.offset(base);
                UnknownFrame unknown{type};
                unknown.parse(r);
#define F(FrameT)                         \
    case frame_typeof<FrameT>: {          \
        FrameT frame;                     \
        if (!frame.parse(r)) {            \
            cb(frame, unknown, true);     \
            r = peek;                     \
            return false;                 \
        }                                 \
        return cb(frame, unknown, false); \
    }
                switch (type) {
                    F(DataFrame)
                    F(HeaderFrame)
                    F(PriorityFrame)
                    F(RstStreamFrame)
                    F(SettingsFrame)
                    F(PushPromiseFrame)
                    F(PingFrame)
                    F(GoAwayFrame)
                    F(WindowUpdateFrame)
                    F(ContinuationFrame)
                    default:
                        cb(unknown, unknown, true);
                        return false;
                }
#undef F
            }

            constexpr size_t frame_len(const Frame& frame) {
#define F(FrameI)              \
    case frame_typeof<FrameI>: \
        return 9 + static_cast<const FrameI&>(frame).data_len();
                switch (frame.type) {
                    F(DataFrame)
                    F(HeaderFrame)
                    F(PriorityFrame)
                    F(RstStreamFrame)
                    F(SettingsFrame)
                    F(PushPromiseFrame)
                    F(PingFrame)
                    F(GoAwayFrame)
                    F(WindowUpdateFrame)
                    F(ContinuationFrame)
                    default:
                        return 0;
                }
#undef F
            }

            constexpr bool render_frame(io::writer& w, const auto& frame) {
                const auto len = 9 + frame.data_len();
                if (w.remain().size() < len) {
                    return false;
                }
                return frame.render(w);
            }

            constexpr bool render_frame(io::writer& w, const Frame& frame) {
#define F(FrameI)              \
    case frame_typeof<FrameI>: \
        return render_frame(w, static_cast<const FrameI&>(frame));
                switch (frame.type) {
                    F(DataFrame)
                    F(HeaderFrame)
                    F(PriorityFrame)
                    F(RstStreamFrame)
                    F(SettingsFrame)
                    F(PushPromiseFrame)
                    F(PingFrame)
                    F(GoAwayFrame)
                    F(WindowUpdateFrame)
                    F(ContinuationFrame)
                    default:
                        return false;
                }
#undef F
            }
        }  // namespace frame
    }      // namespace dnet::http2
}  // namespace utils

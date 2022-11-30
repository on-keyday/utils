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
/*
#include <net/http2/error_type.h>
#include <net/http2/stream_state.h>
*/

namespace utils {
    namespace dnet {

        namespace h2frame {
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
            ENUM_STRING_MSG(FrameType::data, "data")
            ENUM_STRING_MSG(FrameType::header, "header")
            ENUM_STRING_MSG(FrameType::priority, "priority")
            ENUM_STRING_MSG(FrameType::rst_stream, "rst_stream")
            ENUM_STRING_MSG(FrameType::settings, "settings")
            ENUM_STRING_MSG(FrameType::push_promise, "push_promise")
            ENUM_STRING_MSG(FrameType::ping, "ping")
            ENUM_STRING_MSG(FrameType::goaway, "goaway")
            ENUM_STRING_MSG(FrameType::window_update, "window_update")
            ENUM_STRING_MSG(FrameType::continuous, "continuous")
            END_ENUM_STRING_MSG("unknown")

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

            /*
using net::http2::Flag;
using net::http2::FrameType;
using net::http2::H2Error;
using Status = utils::net::http2::Status;*/

            struct Frame {
                int len = 0;
                const FrameType type = FrameType::invalid;
                Flag flag = Flag::none;
                std::int32_t id = 0;

                constexpr Frame() = default;

               protected:
                constexpr Frame(FrameType type)
                    : type(type) {}
            };

            struct Priority {
                std::uint32_t depend = 0;
                std::uint8_t weight = 0;
            };

            struct DataFrame : Frame {
                std::uint8_t padding = 0;
                const char* data = nullptr;
                std::uint32_t datalen = 0;  // extension field
                const char* pad = nullptr;
                constexpr DataFrame()
                    : Frame(FrameType::data) {}
            };

            struct HeaderFrame : Frame {
                std::uint8_t padding = 0;
                Priority priority;
                const char* data = nullptr;
                std::uint32_t datalen;  // extension field
                const char* pad = nullptr;
                constexpr HeaderFrame()
                    : Frame(FrameType::header) {}
            };

            struct PriorityFrame : Frame {
                Priority priority;
                constexpr PriorityFrame()
                    : Frame(FrameType::priority) {}
            };

            struct RstStreamFrame : Frame {
                std::uint32_t code = 0;
                constexpr RstStreamFrame()
                    : Frame(FrameType::rst_stream) {}
            };

            struct SettingsFrame : Frame {
                const char* settings = nullptr;
                constexpr SettingsFrame()
                    : Frame(FrameType::settings) {}
            };

            struct PushPromiseFrame : Frame {
                std::uint8_t padding = 0;
                std::int32_t promise = 0;
                const char* data = nullptr;
                std::uint32_t datalen = 0;  // extension field
                const char* pad = nullptr;
                constexpr PushPromiseFrame()
                    : Frame(FrameType::push_promise) {}
            };

            struct PingFrame : Frame {
                std::uint64_t opaque = 0;
                constexpr PingFrame()
                    : Frame(FrameType::ping) {}
            };

            struct GoAwayFrame : Frame {
                std::int32_t processed_id = 0;
                std::uint32_t code = 0;
                const char* debug_data = nullptr;
                size_t dbglen = 0;  // extension field
                constexpr GoAwayFrame()
                    : Frame(FrameType::goaway) {}
            };

            struct WindowUpdateFrame : Frame {
                std::int32_t increment = 0;
                constexpr WindowUpdateFrame()
                    : Frame(FrameType::window_update) {}
            };

            struct ContinuationFrame : Frame {
                const char* data = nullptr;
                constexpr ContinuationFrame()
                    : Frame(FrameType::continuous) {}
            };

        }  // namespace h2frame

        struct ErrCode {
            // library error code
            int err = 0;
            // http2 error code
            h2frame::H2Error h2err = h2frame::H2Error::none;
            // stream level error
            bool strm = false;
            // debug data
            const char* debug = nullptr;
        };

        using H2Callback = bool (*)(void* user, h2frame::Frame* frame, ErrCode err);
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
        dnet_dll_export(bool) render_frame(char* text, size_t& size, size_t cap, ErrCode& err, h2frame::Frame* frame, bool auto_correct);
        // pass_frame pass a http2 frame
        // this function reads frame header and skips bytes of length
        // this fucntion provide any checks
        // if skip succeedes returns true otherwise returns false
        dnet_dll_export(bool) pass_frame(const char* text, size_t size, size_t& red);
    }  // namespace dnet
}  // namespace utils

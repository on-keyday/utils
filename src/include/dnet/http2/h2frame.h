/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2frame - http2 frame
#pragma once
#include "../dll/dllh.h"
#include <net/http2/error_type.h>
#include <net/http2/stream_state.h>

namespace utils {
    namespace dnet {

        namespace h2frame {
            using net::http2::Flag;
            using net::http2::FrameType;
            using net::http2::H2Error;
            using Status = utils::net::http2::Status;

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

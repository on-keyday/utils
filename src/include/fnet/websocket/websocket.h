/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// websocket - WebSocket implementation
#pragma once
#include "../dll/dllh.h"
#include <strutil/append.h>
#include <view/charvec.h>
#include <view/sized.h>
#include <helper/pushbacker.h>
#include <strutil/readutil.h>
#include <cstdint>
#include <random>
#include <core/byte.h>
#include "../storage.h"
#include <binary/number.h>
#include <helper/defer.h>
#include <binary/flags.h>
#include <unicode/utf/convert.h>
#include "../storage.h"

namespace utils {
    namespace fnet::websocket {

        enum class FrameType {
            continuous = 0x00,
            text = 0x01,
            binary = 0x02,
            closing = 0x08,
            ping = 0x09,
            pong = 0x0A,
        };

        struct FrameFlags {
           private:
            binary::flags_t<byte, 1, 3, 4> flags;
            bits_flag_alias_method(flags, 2, opcode_raw);

           public:
            bits_flag_alias_method(flags, 0, fin);
            bits_flag_alias_method(flags, 1, reserved);
            constexpr FrameType opcode() const {
                return FrameType(opcode_raw());
            }

            constexpr bool set_opcode(FrameType op) {
                return opcode_raw(byte(op));
            }

            constexpr void set(byte h) {
                flags.as_value() = h;
            }

            constexpr byte get() const {
                return flags.as_value();
            }
        };

        struct Frame {
            FrameFlags flags;
            bool masked = false;
            std::uint64_t len = 0;  // only parse
            std::uint32_t maskkey = 0;
            view::rvec data;
        };

        namespace internal {
            constexpr view::wvec cast_to_wvec(view::rvec data) {
                return view::wvec(const_cast<byte*>(data.data()), data.size());
            }
        }  // namespace internal

        constexpr void apply_mask(view::wvec data, std::uint32_t maskkey, auto&& apply_to) {
            binary::Buf<std::uint32_t> keys;
            keys.write_be(maskkey);
            for (size_t i = 0; i < data.size(); i++) {
                data[i] ^= keys[i % 4];
                apply_to(data[i], data[i] ^ keys[i % 4]);
            }
        }

        // write_frame writes websocket frame
        // if no_mask flag is set
        // write_frame interprets frame.data has already been masked with frame.maskkey
        constexpr bool write_frame(auto& buf, const Frame& frame, bool no_mask = false) {
            if (frame.data.null()) {
                return false;
            }
            int len = 0;
            byte lens[9]{};
            binary::writer w{lens};
            if (len <= 125) {
                w.write(byte(len), 1);
                len = 1;
            }
            else if (len <= 0xffff) {
                w.write(126, 1);
                binary::write_num(w, std::uint16_t(len));
                len = 3;
            }
            else {
                w.write(127, 1);
                binary::write_num(w, std::uint64_t(len));
                len = 9;
            }
            if (frame.masked) {
                lens[0] |= 0x80;
            }
            buf.push_back(frame.flags.get());
            strutil::append(buf, view::CharVec(lens, len));
            if (frame.masked) {
                binary::Buf<std::uint32_t> keys;
                keys.write_be(frame.maskkey);
                strutil::append(buf, keys);
                if (no_mask) {
                    strutil::append(buf, frame.data);
                }
                else {
                    for (size_t i = 0; i < frame.len; i++) {
                        buf.push_back(frame.data[i] ^ keys[i % 4]);
                    }
                    apply_mask(internal::cast_to_wvec(frame.data), frame.maskkey, [&](byte&, byte d) {
                        buf.push_back(d);
                    });
                }
            }
            else {
                strutil::append(buf, frame.data);
            }
            return true;
        }

        // read_frame_head reads frame before data payload
        // this function returns true if enough length exists including payload
        // otherwise returns false and red would not updated
        constexpr bool read_frame(binary::reader& r, Frame& frame) {
            auto ofs = r.offset();
            auto rollback = helper::defer([&] {
                r.reset(ofs);
            });
            auto [first_bytes, ok] = r.read(2);
            if (!ok) {
                return false;
            }
            frame.flags.set(first_bytes[0]);
            const byte len = first_bytes[1] & 0x7f;
            frame.masked = bool(first_bytes[1] & 0x80);
            size_t offset = 0;
            if (len <= 125) {
                frame.len = len;
            }
            else if (len == 126) {
                std::uint16_t data;
                if (!binary::read_num(r, data)) {
                    return false;
                }
                frame.len = data;
            }
            else {
                if (len != 127) {
                    return false;  // unexpected!!!
                }
                std::uint64_t data;
                if (!binary::read_num(r, data)) {
                    return false;
                }
                frame.len = data;
            }
            if (frame.masked) {
                if (!binary::read_num(r, frame.maskkey)) {
                    return false;
                }
            }
            if (!r.read(frame.data, frame.len)) {
                return false;
            }
            rollback.cancel();
            return true;
        }

        // tmp_decode_payload decodes payload with a
        void tmp_decode_payload(view::wvec buf, std::uint32_t maskkey, auto&& callback) {
            auto f = [&] {
                apply_mask(buf, maskkey, [](byte& to, byte d) {
                    to = d;
                });
            };
            const auto u = helper::defer(f);
            f();
            callback();
        }

        struct WebSocket {
           private:
            flex_storage input;
            flex_storage output;
            bool server = false;

            std::uint32_t genmask() {
                return std::uint32_t(std::random_device()());
            }

           public:
            void add_input(auto&& data, size_t len) {
                strutil::append(input, view::SizedView(data, len));
            }

            size_t get_output(auto&& buf, size_t limit = ~0, bool peek = false) {
                auto seq = make_ref_seq(output);
                if (output.size() < limit) {
                    limit = output.size();
                }
                strutil::read_n(buf, seq, limit);
                if (!peek) {
                    output.shift_front(seq.rptr);
                }
                return seq.rptr;
            }

            bool write_frame(const Frame& frame, bool no_mask = false) {
                return websocket::write_frame(output, frame, false);
            }

            bool read_frame(auto&& callback, bool peek = false) {
                websocket::Frame frame;
                binary::reader r{input};
                if (!websocket::read_frame(r, frame)) {
                    return false;
                }
                if (frame.masked && peek) {
                    tmp_decode_payload(
                        internal::cast_to_wvec(frame.data),
                        frame.maskkey, [&](auto p) {
                            callback(frame);
                        });
                }
                else {
                    if (frame.masked) {
                        apply_mask(internal::cast_to_wvec(frame.data), frame.maskkey, [](byte& to, byte d) {
                            to = d;
                        });
                    }
                    callback(frame);
                }
                if (!peek) {
                    input.shift_front(r.offset());
                }
                return true;
            }

            // read_frames call read_frame until blocked and consumes
            size_t read_frames(auto&& callback) {
                size_t red = 0;
                while (read_frame(callback)) {
                    red++;
                }
                return red;
            }

            bool write_data(websocket::FrameType type, view::rvec data, bool fin = true) {
                websocket::Frame frame;
                if (!frame.flags.set_opcode(type)) {
                    return false;
                }
                if (!server) {
                    frame.masked = true;
                    frame.maskkey = genmask();
                }
                else {
                    frame.masked = false;
                }
                frame.data = data;
                frame.flags.fin(fin);
                return write_frame(frame);
            }

            bool write_binary(view::rvec data, bool fin = true) {
                return write_data(FrameType::binary, data, fin);
            }

            bool write_text_raw(view::rvec data, bool fin = true) {
                return write_data(FrameType::text, data, fin);
            }

            // this validates UTF-8 text
            bool write_text(view::rvec data, bool fin = true) {
                if (!utf::convert<0, 4>(data, helper::nop, false, false)) {
                    return false;
                }
                return write_text_raw(data, fin);
            }

            bool write_ping(view::rvec data) {
                return write_data(FrameType::ping, data);
            }

            bool write_pong(view::rvec data) {
                return write_data(FrameType::pong, data);
            }

            bool write_close(view::rvec data) {
                return write_data(FrameType::closing, data);
            }

            bool close(std::uint16_t code) {
                binary::Buf<std::uint16_t> data;
                data.write_be(code);
                return write_close(data);
            }

            bool close(std::uint16_t code, view::rvec phrase) {
                flex_storage str;
                str.reserve(2 + phrase.size());
                binary::writer w{str};
                binary::write_num(w, code);
                w.write(phrase);
                return write_close(str);
            }

            void set_server(bool is_server = true) {
                server = is_server;
            }
        };

    }  // namespace fnet::websocket
}  // namespace utils

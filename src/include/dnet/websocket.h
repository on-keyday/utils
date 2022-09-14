/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// websocket - WebSocket implementation
#pragma once
#include "dll/dllh.h"
#include "dll/httpbuf.h"
#include "httpstring.h"
#include "../helper/appender.h"
#include "../helper/view.h"
#include "../helper/pushbacker.h"
#include "../helper/readutil.h"
#include <cstdint>
#include <random>

namespace utils {
    namespace dnet {

        using byte = std::uint8_t;
        namespace websocket {

            enum FrameType {
                empty = 0xF0,
                continuous = 0x00,
                text = 0x01,
                binary = 0x02,
                closing = 0x08,
                ping = 0x09,
                pong = 0x0A,
                mask_fin = 0x80,
                mask_reserved = 0x70,
                mask_opcode = 0x0f,
            };

            struct Frame {
                FrameType type = {};
                bool fin = false;
                bool masked = false;
                size_t len = 0;
                std::uint32_t maskkey = 0;
                const char* data = nullptr;
            };

            // write_frame writes websocket frame
            // if no_mask flag is set
            // write_frame interprets frame.data has already been masked with frame.maskkey
            bool write_frame(auto& buf, const Frame& frame, bool no_mask = false) {
                if (frame.len && !frame.data) {
                    return false;
                }
                const byte opcode = frame.type & websocket::mask_opcode;
                const byte finmask = frame.fin ? mask_fin : 0;
                const byte first_byte = opcode | finmask;
                int len = 0;
                byte lens[9]{};
                auto part = [&](int i) {
                    return byte(std::uint64_t(frame.len >> (8 * (7 - i))) & 0xff);
                };
                if (frame.len <= 125) {
                    lens[0] = byte(frame.len);
                    len = 1;
                }
                else if (frame.len <= 0xffff) {
                    lens[0] = 126;
                    lens[1] = part(0);
                    lens[2] = part(1);
                    len = 3;
                }
                else {
                    lens[0] = 127;
                    lens[1] = part(0);
                    lens[2] = part(1);
                    lens[3] = part(2);
                    lens[4] = part(3);
                    lens[5] = part(4);
                    lens[6] = part(5);
                    lens[7] = part(6);
                    lens[8] = part(7);
                    len = 9;
                }
                if (frame.masked) {
                    lens[0] |= 0x80;
                }
                buf.push_back(first_byte);
                helper::append(buf, helper::SizedView(lens, len));
                if (frame.masked) {
                    byte keys[]{
                        byte((frame.maskkey >> 24) & 0xff),
                        byte((frame.maskkey >> 16) & 0xff),
                        byte((frame.maskkey >> 8) & 0xff),
                        byte((frame.maskkey) & 0xff),
                    };
                    helper::append(buf, helper::SizedView(keys, 4));
                    if (no_mask) {
                        helper::append(buf, helper::SizedView(frame.data, frame.len));
                    }
                    else {
                        for (size_t i = 0; i < frame.len; i++) {
                            buf.push_back(frame.data[i] ^ keys[i % 4]);
                        }
                    }
                }
                else {
                    helper::append(buf, helper::SizedView(frame.data, frame.len));
                }
                return true;
            }

            // read_frame_head reads frame before data payload
            // this function returns true if enough length exists including payload
            // otherwise returns false and red would not updated
            constexpr bool read_frame_head(Frame& frame, auto&& view, size_t size, size_t& red) {
                if (size < red + 2) {
                    return false;
                }
                const byte first_byte = view[red + 0];
                frame.fin = (first_byte & mask_fin) ? true : false;
                frame.type = FrameType(first_byte & mask_opcode);
                const byte mask_len = view[red + 1];
                const byte len = mask_len & 0x7f;
                const bool mask = (mask_len & 0x80 ? true : false);
                frame.masked = mask;
                size_t offset = 0;
                if (len <= 125) {
                    frame.len = len;
                    offset = 2;
                }
                else if (len == 126) {
                    if (size < red + 2 + 2) {
                        return false;
                    }
                    frame.len = std::uint64_t(byte(view[0])) << 8 | byte(view[1]);
                    offset = 4;
                }
                else {
                    if (len != 127) {
                        int* ptr = nullptr;
                        *ptr = 0;  // terminate
                    }
                    if (size < red + 2 + 8) {
                        return false;
                    }
                    auto shift = [&](auto i) -> std::uint64_t {
                        return std::uint64_t(byte(view[red + 2 + i])) << (8 * (7 - i));
                    };
                    frame.len = shift(0) | shift(1) | shift(2) | shift(3) |
                                shift(4) | shift(5) | shift(6) | shift(7);
                    offset = 10;
                }
                if (mask) {
                    if (size < red + offset) {
                        return false;
                    }
                    auto shift = [&](auto i) -> std::uint32_t {
                        return std::uint32_t(byte(view[red + offset + i])) << (8 * (3 - i));
                    };
                    frame.maskkey = shift(0) | shift(1) | shift(2) | shift(3);
                    offset += 4;
                }
                if (size < red + offset + frame.len) {
                    return false;
                }
                red += offset;
                return true;
            }

            constexpr bool read_frame(Frame& frame, const char* view, size_t size, size_t& red) {
                if (!read_frame_head(frame, view, size, red)) {
                    return false;
                }
                frame.data = view + red;
                red += frame.len;
                return true;
            }

            void decode_payload(auto& buf, auto&& view, size_t len, std::uint32_t maskkey) {
                byte keys[]{
                    byte((maskkey >> 24) & 0xff),
                    byte((maskkey >> 16) & 0xff),
                    byte((maskkey >> 8) & 0xff),
                    byte((maskkey)&0xff),
                };
                for (size_t i = 0; i < len; i++) {
                    buf.push_back(view[i] ^ keys[i % 4]);
                }
            }

            // tmp_decode_payload decodes payload with a
            void tmp_decode_payload(char* dec, size_t len, std::uint32_t maskkey, auto&& callback) {
                if (!dec || !len) {
                    return;
                }
                struct Uncode {
                    char* dec;
                    size_t len;
                    std::uint32_t maskkey;
                    ~Uncode() {
                        auto vec = helper::CharVecPushbacker(dec, len);
                        decode_payload(vec, dec, len, maskkey);
                    }
                } u{dec, len, maskkey};
                auto vec = helper::CharVecPushbacker(dec, len);
                decode_payload(vec, dec, len, maskkey);
                callback(dec, len);
            }
        }  // namespace websocket

        struct WebSocket {
           private:
            HTTPBuf input;
            HTTPBuf output;
            bool server = false;

            std::uint32_t genmask() {
                return std::uint32_t(std::random_device()());
            }

           public:
            void add_input(auto&& data, size_t len) {
                String in;
                BorrowString _{input, in};
                helper::append(in, helper::RefSizedView(data, len));
            }

            size_t get_output(auto&& buf, size_t limit = ~0, bool peek = false) {
                String out;
                BorrowString _{output, out};
                auto seq = make_ref_seq(out);
                if (out.size() < limit) {
                    limit = out.size();
                }
                helper::read_n(buf, seq, limit);
                if (!peek) {
                    out.shift(seq.rptr);
                }
                return seq.rptr;
            }

            bool write_frame(const websocket::Frame& frame, bool no_mask = false) {
                String out;
                BorrowString _{output, out};
                return websocket::write_frame(out, frame, false);
            }

            bool read_frame(auto&& callback, bool peek = false) {
                String in;
                BorrowString _{input, in};
                size_t red = 0;
                websocket::Frame frame;
                if (!websocket::read_frame(frame, in.text(), in.size(), red)) {
                    return false;
                }
                if (frame.masked) {
                    websocket::tmp_decode_payload(
                        in.text() + red - frame.len,
                        frame.len, frame.maskkey, [&](auto dec, auto len) {
                            callback(frame);
                        });
                }
                else {
                    callback(frame);
                }
                if (!peek) {
                    in.shift(red);
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

            bool write_data(websocket::FrameType type, const char* data, size_t len, bool fin = true) {
                websocket::Frame frame;
                frame.type = type;
                if (!server) {
                    frame.masked = true;
                    frame.maskkey = genmask();
                }
                frame.data = data;
                frame.len = len;
                frame.fin = fin;
                return write_frame(frame);
            }

            bool write_binary(const char* data, size_t len, bool fin = true) {
                return write_data(websocket::binary, data, len, fin);
            }

            bool write_text(const char* data, size_t len, bool fin = true) {
                return write_data(websocket::text, data, len, fin);
            }

            bool write_text(const char* data, bool fin = true) {
                return write_text(data, data ? utils::strlen(data) : 0, fin);
            }

            bool write_ping(const char* data, size_t len) {
                return write_data(websocket::ping, data, len);
            }

            bool write_pong(const char* data, size_t len) {
                return write_data(websocket::pong, data, len);
            }

            bool write_close(const char* data, size_t len) {
                return write_data(websocket::closing, data, len);
            }

            bool close(std::uint16_t code) {
                byte data[]{
                    byte((code >> 8) & 0xff),
                    byte((code)&0xff),
                };
                return write_close((const char*)data, 2);
            }

            bool close(std::uint16_t code, const char* phrase, size_t len) {
                String str;
                str.push_back((code >> 8) & 0xff);
                str.push_back((code)&0xff);
                helper::append(str, helper::SizedView(phrase, len));
                return write_close(str.begin(), str.size());
            }

            void set_server(bool is_server = true) {
                server = is_server;
            }
        };
    }  // namespace dnet
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// websocket - WebSocket implementation
#pragma once
#include "dll/dllh.h"
#include "../helper/appender.h"
#include "../view/charvec.h"
#include "../helper/pushbacker.h"
#include "../helper/readutil.h"
#include <cstdint>
#include <random>
#include "../core/byte.h"
#include "storage.h"
#include "../io/number.h"

namespace utils {
    namespace dnet {

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
                size_t len = 0;  // only parse
                std::uint32_t maskkey = 0;
                view::rvec data;
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
                io::writer w{lens};
                const auto len = frame.data.size();
                if (len <= 125) {
                    w.write(byte(len), 1);
                    len = 1;
                }
                else if (len <= 0xffff) {
                    w.write(126, 1);
                    io::write_num(w, std::uint16_t(len));
                    len = 3;
                }
                else {
                    w.write(127, 1);
                    io::write_num(w, std::uint64_t(len));
                    len = 9;
                }
                if (frame.masked) {
                    lens[0] |= 0x80;
                }
                buf.push_back(first_byte);
                helper::append(buf, view::CharVec(lens, len));
                if (frame.masked) {
                    endian::Buf<std::uint32_t> keys;
                    keys.write_be(frame.maskkey);
                    helper::append(buf, keys);
                    if (no_mask) {
                        helper::append(buf, frame.data);
                    }
                    else {
                        for (size_t i = 0; i < frame.len; i++) {
                            buf.push_back(frame.data[i] ^ keys[i % 4]);
                        }
                    }
                }
                else {
                    helper::append(buf, frame.data);
                }
                return true;
            }

            // read_frame_head reads frame before data payload
            // this function returns true if enough length exists including payload
            // otherwise returns false and red would not updated
            constexpr bool read_frame_head(Frame& frame, io::reader& r) {
                auto [first_bytes, ok] = r.read(2);
                if (!ok) {
                    return false;
                }
                const byte first_byte = first_bytes[0];
                frame.fin = (first_byte & mask_fin) ? true : false;
                frame.type = FrameType(first_byte & mask_opcode);
                const byte mask_len = first_bytes[1];
                const byte len = mask_len & 0x7f;
                const bool mask = (mask_len & 0x80 ? true : false);
                frame.masked = mask;
                size_t offset = 0;
                if (len <= 125) {
                    frame.len = len;
                }
                else if (len == 126) {
                    std::uint16_t data;
                    if (!io::read_num(r, data)) {
                        return false;
                    }
                    frame.len = data;
                }
                else {
                    if (len != 127) {
                        int* ptr = nullptr;
                        *ptr = 0;  // terminate
                    }
                    std::uint64_t data;
                    if (!io::read_num(r, data)) {
                        return false;
                    }
                    frame.len = data;
                }
                if (mask) {
                    if (!io::read_num(r, frame.maskkey)) {
                        return false;
                    }
                }
                if (!r.read(frame.data, frame.len)) {
                    return false;
                }
                return true;
            }

            void decode_payload(auto& buf, auto&& view, size_t len, std::uint32_t maskkey) {
                endian::Buf<std::uint32_t> keys;
                keys.write_be(maskkey);
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
            flex_storage input;
            flex_storage output;
            bool server = false;

            std::uint32_t genmask() {
                return std::uint32_t(std::random_device()());
            }

           public:
            void add_input(auto&& data, size_t len) {
                helper::append(input, helper::RefSizedView(data, len));
            }

            size_t get_output(auto&& buf, size_t limit = ~0, bool peek = false) {
                auto seq = make_ref_seq(output);
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
                return websocket::write_frame(output, frame, false);
            }

            bool read_frame(auto&& callback, bool peek = false) {
                size_t red = 0;
                websocket::Frame frame;
                if (!websocket::read_frame(frame, input.text(), input.size(), red)) {
                    return false;
                }
                if (frame.masked) {
                    websocket::tmp_decode_payload(
                        input.text() + red - frame.len,
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

            bool write_data(websocket::FrameType type, view::rvec data, bool fin = true) {
                websocket::Frame frame;
                frame.type = type;
                if (!server) {
                    frame.masked = true;
                    frame.maskkey = genmask();
                }
                else {
                    frame.masked = false;
                }
                frame.data = data;
                frame.fin = fin;
                return write_frame(frame);
            }

            bool write_binary(view::rvec data, bool fin = true) {
                return write_data(websocket::binary, data, fin);
            }

            bool write_text(view::rvec data, bool fin = true) {
                return write_data(websocket::text, data, fin);
            }

            bool write_ping(view::rvec data) {
                return write_data(websocket::ping, data);
            }

            bool write_pong(view::rvec data) {
                return write_data(websocket::pong, data);
            }

            bool write_close(view::rvec data) {
                return write_data(websocket::closing, data);
            }

            bool close(std::uint16_t code) {
                endian::Buf<std::uint16_t> data;
                data.write_be(code);
                return write_close(data);
            }

            bool close(std::uint16_t code, view::rvec phrase) {
                flex_storage str;
                str.reserve(2 + phrase.size());
                io::writer w{str};
                io::write_num(w, code);
                w.write(phrase);
                return write_close(str);
            }

            void set_server(bool is_server = true) {
                server = is_server;
            }
        };
    }  // namespace dnet
}  // namespace utils

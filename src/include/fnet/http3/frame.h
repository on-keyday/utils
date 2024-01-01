/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../quic/varint.h"

namespace futils {
    namespace fnet::http3::frame {
        enum class Type {
            DATA = 0x00,
            HEADER = 0x01,
            RESERVED_1 = 0x02,
            CANCEL_PUSH = 0x03,
            SETTINGS = 0x04,
            PUSH_PROMISE = 0x05,
            RESERVED_2 = 0x06,
            GOAWAY = 0x07,
            RESERVED_3 = 0x08,
            RESERVED_4 = 0x09,
            MAX_PUSH_ID = 0x0d,
        };

        struct FrameHeader {
            quic::varint::Value type;
            quic::varint::Value length;

            constexpr bool parse(binary::reader& r) {
                return quic::varint::read(r, type) &&
                       quic::varint::read(r, length);
            }

            constexpr bool render(binary::writer& w) const {
                return quic::varint::write(w, type) &&
                       quic::varint::write(w, length);
            }

            constexpr bool parse_type_len(binary::reader& r, Type t) {
                return parse(r) &&
                       type == std::uint64_t(t);
            }

            constexpr bool redner_type_len(binary::writer& w, Type type, quic::varint::Value len) const {
                return quic::varint::write(w, std::uint64_t(type)) &&
                       quic::varint::write(w, len);
            }
        };

        using FrameHeaderArea = byte[16];

        view::wvec get_header(FrameHeaderArea& area, Type type, std::uint64_t length) {
            frame::FrameHeader head;
            head.type = std::uint64_t(type);
            head.length = length;
            binary::writer w{area};
            if (!head.render(w)) {
                return {};
            }
            return w.written();
        }

        struct Data : FrameHeader {
            view::rvec data;
            constexpr bool parse(binary::reader& r) {
                return FrameHeader::parse_type_len(r, Type::DATA) &&
                       r.read(data, length);
            }

            constexpr bool render(binary::writer& w) const {
                return redner_type_len(w, Type::DATA, data.size()) &&
                       w.write(data);
            }
        };

        struct Headers : FrameHeader {
            view::rvec encoded_field;
            constexpr bool parse(binary::reader& r) {
                return FrameHeader::parse_type_len(r, Type::HEADER) &&
                       r.read(encoded_field, length);
            }

            constexpr bool render(binary::writer& w) const {
                return redner_type_len(w, Type::HEADER, encoded_field.size()) &&
                       w.write(encoded_field);
            }
        };

        struct CancelPush : FrameHeader {
            quic::varint::Value push_id;

            constexpr bool parse(binary::reader& r) {
                return FrameHeader::parse_type_len(r, Type::CANCEL_PUSH) &&
                       quic::varint::read(r, push_id) &&
                       push_id.len == length;
            }

            constexpr bool render(binary::writer& w) const {
                return redner_type_len(w, Type::CANCEL_PUSH, quic::varint::len(push_id)) &&
                       quic::varint::write(w, push_id);
            }
        };

        struct Setting {
            quic::varint::Value id;
            quic::varint::Value value;

            constexpr bool parse(binary::reader& r) {
                return quic::varint::read(r, id) &&
                       quic::varint::read(r, value);
            }

            constexpr bool render(binary::writer& w) const {
                return quic::varint::write(w, id) &&
                       quic::varint::write(w, value);
            }
        };

        struct Settings : FrameHeader {
            view::rvec settings;

            constexpr bool parse(binary::reader& r) {
                return FrameHeader::parse_type_len(r, Type::SETTINGS) &&
                       r.read(settings, length);
            }

            constexpr bool visit_settings(auto&& visit) const {
                binary::reader r{settings};
                Setting s;
                while (!r.empty()) {
                    if (!s.parse(r)) {
                        return false;
                    }
                    if (!visit(s)) {
                        return false;
                    }
                }
                return true;
            }

            constexpr bool render(binary::writer& w) const {
                return redner_type_len(w, Type::SETTINGS, settings.size()) &&
                       w.write(settings);
            }
        };

        struct PushPromise : FrameHeader {
            quic::varint::Value push_id;
            view::rvec encoded_field;
            constexpr bool parse(binary::reader& r) {
                return FrameHeader::parse_type_len(r, Type::PUSH_PROMISE) &&
                       quic::varint::read(r, push_id) &&
                       push_id.len <= length &&
                       r.read(encoded_field, length - push_id.len);
            }

            constexpr bool render(binary::writer& w) const {
                return redner_type_len(w, Type::PUSH_PROMISE, quic::varint::len(push_id) + encoded_field.size()) &&
                       quic::varint::write(w, push_id) &&
                       w.write(encoded_field);
            }
        };

        struct GoAway : FrameHeader {
            quic::varint::Value id;
            constexpr bool parse(binary::reader& r) {
                return FrameHeader::parse_type_len(r, Type::GOAWAY) &&
                       quic::varint::read(r, id) &&
                       id.len == length;
            }

            constexpr bool render(binary::writer& w) const {
                return redner_type_len(w, Type::GOAWAY, quic::varint::len(id)) &&
                       quic::varint::write(w, id);
            }
        };

        struct MaxPushID : FrameHeader {
            quic::varint::Value push_id;
            constexpr bool parse(binary::reader& r) {
                return FrameHeader::parse_type_len(r, Type::MAX_PUSH_ID) &&
                       quic::varint::read(r, push_id) &&
                       push_id.len == length;
            }

            constexpr bool render(binary::writer& w) const {
                return redner_type_len(w, Type::MAX_PUSH_ID, quic::varint::len(push_id)) &&
                       quic::varint::write(w, push_id);
            }
        };
    }  // namespace fnet::http3::frame
}  // namespace futils

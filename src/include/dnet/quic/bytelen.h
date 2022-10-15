/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// bytelen -ByteLen
#pragma once
#include <cstdint>
#include <type_traits>

namespace utils {
    namespace dnet {
        namespace quic {
            using byte = unsigned char;

            enum class PacketType {
                Initial = 0x0,
                ZeroRTT = 0x1,  // 0-RTT
                HandShake = 0x2,
                Retry = 0x3,
                VersionNegotiation,  // section 17.2.1

                OneRTT,  // 1-RTT

            };

            struct PacketFlags {
                byte* value = nullptr;

                constexpr byte raw() const {
                    return value ? *value : 0xff;
                }

                constexpr bool invalid() const {
                    return (raw() & 0x40) == 0;
                }

                constexpr bool is_short() const {
                    return (raw() & 0x80) == 0;
                }

                constexpr bool is_long() const {
                    return !is_short() && !invalid();
                }

                constexpr PacketType type() const {
                    if (invalid()) {
                        return PacketType::VersionNegotiation;
                    }
                    if (is_short()) {
                        return PacketType::OneRTT;
                    }
                    byte long_header_type = (raw() & 0x30) >> 4;
                    return PacketType{long_header_type};
                }

                constexpr byte spec_mask() const {
                    switch (type()) {
                        case PacketType::VersionNegotiation:
                            return 0x7f;
                        case PacketType::OneRTT:
                            return 0x3f;
                        default:
                            return 0x0f;
                    }
                }

                constexpr byte flags() const {
                    return raw() & spec_mask();
                }

                constexpr byte protect_mask() const {
                    if (is_short()) {
                        return 0x1f;
                    }
                    else {
                        return 0x0f;
                    }
                }

                constexpr void protect(byte with) {
                    if (!value) {
                        return;
                    }
                    *value ^= (with & protect_mask());
                }

                constexpr byte packet_number_length() const {
                    return (raw() & 0x3) + 1;
                }
            };

            enum class FrameType {
                PADDING,                                 // section 19.1
                PING,                                    // section 19.2
                ACK,                                     // section 19.3
                ACK_ECN,                                 // with ECN count
                RESET_STREAM,                            // section 19.4
                STOP_SENDING,                            // section 19.5
                CRYPTO,                                  // section 19.6
                NEW_TOKEN,                               // section 19.7
                STREAM,                                  // section 19.8
                MAX_DATA = 0x10,                         // section 19.9
                MAX_STREAM_DATA,                         // section 19.10
                MAX_STREAMS,                             // section 19.11
                MAX_STREAMS_BIDI = MAX_STREAMS,          // section 19.11
                MAX_STREAMS_UNI,                         // section 19.11
                DATA_BLOCKED,                            // section 19.12
                STREAM_DATA_BLOCKED,                     // section 19.13
                STREAMS_BLOCKED,                         // section 19.14
                STREAMS_BLOCKED_BIDI = STREAMS_BLOCKED,  // section 19.14
                STREAMS_BLOCKED_UNI,                     // section 19.14
                NEW_CONNECTION_ID,                       // section 19.15
                RETIRE_CONNECTION_ID,                    // seciton 19.16
                PATH_CHALLENGE,                          // seciton 19.17
                PATH_RESPONSE,                           // seciton 19.18
                CONNECTION_CLOSE,                        // seciton 19.19
                CONNECTION_CLOSE_APP,                    // application reason section 19.19
                HANDSHAKE_DONE,                          // seciton 19.20
                DATAGRAM = 0x30,                         // rfc 9221 datagram
                DATAGRAM_LEN,
            };

            constexpr bool is_STREAM(FrameType type) {
                constexpr auto st = int(FrameType::STREAM);
                const auto ty = int(type);
                return st <= ty && ty <= (st | 0x7);
            }

            constexpr bool is_MAX_STREAMS(FrameType type) {
                return type == FrameType::MAX_STREAMS_BIDI ||
                       type == FrameType::MAX_STREAMS_UNI;
            }

            constexpr bool is_STREAMS_BLOCKED(FrameType type) {
                return type == FrameType::STREAMS_BLOCKED_BIDI ||
                       type == FrameType::STREAMS_BLOCKED_UNI;
            }

            constexpr bool is_ACK(FrameType type) {
                return type == FrameType::ACK ||
                       type == FrameType::ACK_ECN;
            }

            constexpr bool is_CONNECTION_CLOSE(FrameType type) {
                return type == FrameType::CONNECTION_CLOSE ||
                       type == FrameType::CONNECTION_CLOSE_APP;
            }

            constexpr bool is_DATAGRAM(FrameType type) {
                return type == FrameType::DATAGRAM ||
                       type == FrameType::DATAGRAM_LEN;
            }

            constexpr bool is_ACKEliciting(FrameType type) {
                return type != FrameType::ACK &&
                       type != FrameType::ACK_ECN &&
                       type != FrameType::CONNECTION_CLOSE &&
                       type != FrameType::CONNECTION_CLOSE_APP;
            }

            struct FrameFlags {
                byte* value = nullptr;

                constexpr byte raw() const {
                    return value ? *value : 0;
                }

                constexpr FrameType type_detail() const {
                    return FrameType(raw());
                }

                constexpr FrameType type() const {
                    auto ty = type_detail();
                    if (is_STREAM(ty)) {
                        return FrameType::STREAM;
                    }
                    if (is_MAX_STREAMS(ty)) {
                        return FrameType::MAX_STREAMS;
                    }
                    if (is_CONNECTION_CLOSE(ty)) {
                        return FrameType::CONNECTION_CLOSE;
                    }
                    if (is_STREAMS_BLOCKED(ty)) {
                        return FrameType::STREAMS_BLOCKED;
                    }
                    if (is_ACK(ty)) {
                        return FrameType::ACK;
                    }
                    if (is_DATAGRAM(ty)) {
                        return FrameType::DATAGRAM;
                    }
                    return ty;
                }

                constexpr bool STREAM_fin() const {
                    if (type() == FrameType::STREAM) {
                        return raw() & 0x01;
                    }
                    return false;
                }

                constexpr bool STREAM_len() const {
                    if (type() == FrameType::STREAM) {
                        return raw() & 0x2;
                    }
                    return false;
                }

                constexpr bool STREAM_off() const {
                    if (type() == FrameType::STREAM) {
                        return raw() & 0x4;
                    }
                    return false;
                }

                constexpr bool DATAGRAM_len() const {
                    return type_detail() == FrameType::DATAGRAM_LEN;
                }
            };

            struct ByteLen {
                byte* data = nullptr;
                size_t len = 0;

                constexpr bool enough(size_t l) const {
                    return data && len >= l;
                }
                constexpr bool valid() const {
                    return enough(0);
                }

                constexpr bool adjacent(const ByteLen& v) const {
                    if (!valid() || !v.valid()) {
                        return false;
                    }
                    return data + len == v.data;
                }

                template <class T>
                constexpr bool as(T& value, size_t offset = 0, bool big_endian = true) const {
                    if (!enough((offset + 1) * sizeof(T))) {
                        return false;
                    }
                    using Result = std::make_unsigned_t<T>;
                    Result result{};
                    auto ofs = offset * sizeof(T);
                    if (big_endian) {
                        for (auto i = 0; i < sizeof(T); i++) {
                            result |= Result(data[i + ofs]) << (8 * (sizeof(T) - 1 - i));
                        }
                    }
                    else {
                        for (auto i = 0; i < sizeof(T); i++) {
                            result |= Result(data[i + ofs]) << (8 * i);
                        }
                    }
                    value = result;
                    return true;
                }

                constexpr bool packet_number(std::uint32_t& pn, size_t len) {
                    if (len == 0 || len > 4) {
                        return false;
                    }
                    if (!enough(len)) {
                        return false;
                    }
                    if (len == 1) {
                        pn = data[0];
                    }
                    else if (len == 2) {
                        pn = std::uint32_t(data[0]) << 8 | data[1];
                    }
                    else if (len == 3) {
                        pn = std::uint32_t(data[0]) << 16 | std::uint32_t(data[1]) << 8 | data[2];
                    }
                    else {
                        pn = std::uint32_t(data[0]) << 24 | std::uint32_t(data[1]) << 16 |
                             std::uint32_t(data[2]) << 8 | data[3];
                    }
                    return true;
                }

                template <class T>
                constexpr T as(size_t offset = 0, bool big_endian = true) const {
                    T val{};
                    as(val, offset, big_endian);
                    return val;
                }

                constexpr size_t varintlen() const {
                    if (!enough(1)) {
                        return 0;
                    }
                    auto mask = data[0] & 0xC0;
                    if (mask == 0x00) {
                        return 1;
                    }
                    if (mask == 0x40) {
                        return 2;
                    }
                    if (mask == 0x80) {
                        return 4;
                    }
                    if (mask == 0xC0) {
                        return 8;
                    }
                    return 0;
                }

                constexpr bool varint(size_t& value) const {
                    auto len = varintlen();
                    if (len == 0) {
                        return false;
                    }
                    if (len == 1) {
                        value = data[0];
                        return true;
                    }
                    if (!enough(len)) {
                        return false;
                    }
                    if (len == 2) {
                        std::uint16_t val = 0;
                        if (!as(val)) {
                            return false;
                        }
                        val &= ~0xC000;
                        value = val;
                        return true;
                    }
                    if (len == 4) {
                        std::uint32_t val = 0;
                        if (!as(val)) {
                            return false;
                        }
                        val &= ~0xC0000000;
                        value = val;
                        return true;
                    }
                    if (len == 8) {
                        std::uint64_t val = 0;
                        if (!as(val)) {
                            return false;
                        }
                        val &= ~0xC000000000000000;
                        value = val;
                        return true;
                    }
                    return false;
                }

                constexpr size_t varint() const {
                    size_t value = 0;
                    varint(value);
                    return value;
                }

                constexpr bool forward(ByteLen& blen, size_t offset) const {
                    if (!enough(offset)) {
                        return false;
                    }
                    blen = ByteLen{data + offset, len - offset};
                    return true;
                }

                constexpr ByteLen forward(size_t offset) const {
                    ByteLen blen{};
                    forward(blen, offset);
                    return blen;
                }

                constexpr bool resized(ByteLen& blen, size_t resize) const {
                    if (!enough(resize)) {
                        return false;
                    }
                    blen = ByteLen{data, resize};
                    return true;
                }

                constexpr ByteLen resized(size_t resize) const {
                    ByteLen blen{};
                    resized(blen, resize);
                    return blen;
                }

                constexpr bool fwdenough(size_t offset, size_t expect) {
                    *this = this->forward(offset);
                    return enough(expect);
                }

                constexpr bool fwdresize(ByteLen& to, size_t offset, size_t expect) {
                    if (!fwdenough(offset, expect)) {
                        return false;
                    }
                    to = resized(expect);
                    return true;
                }

                constexpr size_t varintfwd(size_t& to) {
                    auto len = varintlen();
                    if (!varint(to)) {
                        return 0;
                    }
                    *this = forward(len);
                    return len;
                }

                constexpr bool varintfwd(ByteLen& to) {
                    auto len = varintlen();
                    if (!enough(len)) {
                        return 0;
                    }
                    to = resized(len);
                    *this = forward(len);
                    return true;
                }

                constexpr bool is_varint_valid() const {
                    auto vlen = varintlen();
                    if (vlen == 0 || vlen > len) {
                        return false;
                    }
                    return true;
                }
            };

            struct WPacket {
                ByteLen b;
                size_t offset = 0;
                bool overflow = false;

                constexpr void append(const byte* src, size_t len) {
                    for (auto i = 0; i < len; i++) {
                        if (b.len <= offset) {
                            overflow = true;
                            break;
                        }
                        b.data[offset] = src[i];
                        offset++;
                    }
                }

                constexpr void add(byte src, size_t len) {
                    for (auto i = 0; i < len; i++) {
                        if (b.len <= offset) {
                            overflow = true;
                            break;
                        }
                        b.data[offset] = src;
                        offset++;
                    }
                }

                constexpr ByteLen acquire(size_t len) {
                    if (b.len < offset + len) {
                        return {};
                    }
                    auto res = ByteLen{b.data + offset, len};
                    offset += len;
                    return res;
                }

                constexpr ByteLen zeromem(size_t len) {
                    auto buf = acquire(len);
                    if (!buf.enough(len)) {
                        return {};
                    }
                    for (auto i = 0; i < len; i++) {
                        buf.data[i] = 0;
                    }
                    return buf;
                }

                template <class T>
                constexpr ByteLen as(T val, bool big_endian = true) {
                    auto buf = acquire(sizeof(T));
                    if (!buf.enough(sizeof(T))) {
                        return {};
                    }
                    std::make_unsigned_t<T> src = val;
                    if (big_endian) {
                        for (auto i = 0; i < sizeof(T); i++) {
                            buf.data[i] = (src >> (8 * (sizeof(T) - 1 - i))) & 0xff;
                        }
                    }
                    else {
                        for (auto i = 0; i < sizeof(T); i++) {
                            buf.data[i] = (src >> (8 * i)) & 0xff;
                        }
                    }
                    return buf;
                }

                constexpr byte* as_byte(size_t val) {
                    if (val > 0xff) {
                        return nullptr;
                    }
                    return as<byte>(val).data;
                }

                constexpr ByteLen varint(size_t val) {
                    constexpr auto masks = 0xC000000000000000;
                    if (val & masks) {
                        return {};
                    }
                    val &= ~masks;
                    size_t len = 0;
                    byte mask;
                    if (val < 0x40) {
                        len = 1;
                        mask = 0;
                    }
                    else if (val < 0x4000) {
                        len = 2;
                        mask = 0x40;
                    }
                    else if (val < 0x40000000) {
                        len = 4;
                        mask = 0x80;
                    }
                    else {
                        len = 8;
                        mask = 0xC0;
                    }
                    auto buf = acquire(len);
                    if (!buf.enough(len)) {
                        return {};
                    }
                    if (len == 1) {
                        buf.data[0] = byte(val);
                        return buf;
                    }
                    for (auto i = 0; i < len; i++) {
                        buf.data[i] = (val >> (8 * (len - 1 - i))) & 0xff;
                        if (i == 0) {
                            buf.data[i] |= mask;
                        }
                    }
                    return buf;
                }

                FrameFlags frame_type(FrameType ty) {
                    auto len = acquire(1);
                    if (!len.enough(1)) {
                        return {};
                    }
                    *len.data = byte(ty);
                    return FrameFlags{len.data};
                }

                PacketFlags packet_flags(byte raw) {
                    auto len = acquire(1);
                    if (!len.enough(1)) {
                        return {};
                    }
                    *len.data = raw;
                    return PacketFlags{len.data};
                }

                PacketFlags packet_flags(PacketType type, byte pn_length, bool spin = false, bool key_phase = false) {
                    byte raw = 0;
                    auto lentomask = [&] {
                        return byte(pn_length - 1);
                    };
                    auto valid_pnlen = [&] {
                        return 1 <= pn_length && pn_length <= 4;
                    };
                    if (type == PacketType::Initial) {
                        if (!valid_pnlen()) {
                            return {};
                        }
                        raw = 0xC0 | lentomask();
                    }
                    else if (type == PacketType::ZeroRTT) {
                        if (!valid_pnlen()) {
                            return {};
                        }
                        raw = 0xD0 | lentomask();
                    }
                    else if (type == PacketType::HandShake) {
                        if (!valid_pnlen()) {
                            return {};
                        }
                        raw = 0xE0 | lentomask();
                    }
                    else if (type == PacketType::Retry) {
                        if (!valid_pnlen()) {
                            return {};
                        }
                        raw = 0xF0 | lentomask();
                    }
                    else if (type == PacketType::VersionNegotiation) {
                        raw = 0x80;
                    }
                    else if (type == PacketType::OneRTT) {
                        if (!valid_pnlen()) {
                            return {};
                        }
                        raw = 0x40 | (spin ? 0x20 : 0) | (key_phase ? 0x04 : 0) | lentomask();
                    }
                    else {
                        return {};
                    }
                    return packet_flags(raw);
                }

                constexpr ByteLen packet_number(std::uint32_t value, byte len) {
                    if (len == 0 || len > 0x4) {
                        return {};
                    }
                    auto num = acquire(len);
                    if (!num.enough(len)) {
                        return {};
                    }
                    if (len == 1) {
                        num.data[0] = value & 0xff;
                    }
                    else if (len == 2) {
                        num.data[0] = (value >> 8) & 0xff;
                        num.data[1] = value & 0xff;
                    }
                    else if (len == 3) {
                        num.data[0] = (value >> 16) & 0xff;
                        num.data[1] = (value >> 8) & 0xff;
                        num.data[2] = value & 0xff;
                    }
                    else {
                        num.data[0] = (value >> 24) & 0xff;
                        num.data[1] = (value >> 16) & 0xff;
                        num.data[2] = (value >> 8) & 0xff;
                        num.data[3] = value & 0xff;
                    }
                    return num;
                }
            };

            struct PacketInfo {
                // head is header of QUIC packet without packet number
                ByteLen head;
                // payload is payload of QUIC packet including packet number
                // payload.data must be adjacent to head.data
                ByteLen payload;
                // dstID is destination connection ID of QUIC
                ByteLen dstID;
                // processed_payload is encrypted/decrypted payload without packet number
                ByteLen processed_payload;
            };

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

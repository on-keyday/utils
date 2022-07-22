/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_head - quic head of packet header
#pragma once

#include "../common/variable_int.h"
#include "../conn/connId.h"
#include <type_traits>
#include <concepts>

namespace utils {
    namespace quic {
        namespace packet {

            enum class Error {
                none,
                invalid_argument,
                invalid_fbinfo,
                not_enough_length,
                consistency_error,
                decode_error,
                unimplemented_required,  // unimplemented and must fix
                memory_exhausted,
                too_long_connection_id,
                unknown_connection_id,
            };

            struct FirstByte {
                byte raw;
                tsize offset;

                constexpr bool invalid() const {
                    return (raw & 0x40) == 0;
                }

                constexpr bool is_short() const {
                    return (raw & 0x80) == 0;
                }

                constexpr bool is_long() const {
                    return !is_short() && !invalid();
                }

                constexpr types type() const {
                    if (invalid()) {
                        return types::VersionNegotiation;
                    }
                    if (is_short()) {
                        return types::OneRTT;
                    }
                    byte long_header_type = (raw & 0x30) >> 4;
                    return types{long_header_type};
                }

                constexpr byte spec_mask() const {
                    switch (type()) {
                        case types::VersionNegotiation:
                            return 0x7f;
                        case types::OneRTT:
                            return 0x3f;
                        default:
                            return 0x0f;
                    }
                }

                constexpr byte flags() const {
                    return raw & spec_mask();
                }

                constexpr byte protect_mask() const {
                    if (is_short()) {
                        return 0x1f;
                    }
                    else {
                        return 0x0f;
                    }
                }

                constexpr void protect(byte of) {
                    raw ^= (of & protect_mask());
                }

                constexpr byte packet_number_length() {
                    return (raw & 0x3) + 1;
                }
            };

            constexpr FirstByte guess_packet(byte first_byte) {
                return FirstByte{first_byte};
            }

            // common parameter of almost all packet and help parse
            // for exception, RetryPacket has no payload and no packet_number
            struct Packet {
                FirstByte flags;
                tsize dstID_len;
                byte* dstID;

                // protected
                tsize packet_number;
                byte* raw_header;
                byte* raw_payload;
                tsize payload_length;
                tsize remain;

                // decrepted
                bytes::Bytes decrypted_payload;
                tsize decrypted_length;

                tsize end_offset;
            };

            /*
            Long Header Packet {
                Header Form (1) = 1,
                Fixed Bit (1) = 1,
                Long Packet Type (2),
                Type-Specific Bits (4),  // ~ 1 byte
                Version (32),    // 4 byte
                Destination Connection ID Length (8),
                Destination Connection ID (0..160),
                Source Connection ID Length (8),
                Source Connection ID (0..160),
                Type-Specific Payload (..),
            }
            */
            struct LongPacket : Packet {
                uint version;
                tsize srcID_len;
                byte* srcID;
            };

            /*
            1-RTT Packet {
                Header Form (1) = 0,
                Fixed Bit (1) = 1,
                Spin Bit (1),
                Reserved Bits (2),
                Key Phase (1),
                Packet Number Length (2), // 1 byte
                Destination Connection ID (0..160), // 0 ~ 20 byte (size known)
                Packet Number (8..32), // 1 ~ 4 byte
                Packet Payload (8..), // 1 byte ~
            }
            */
            struct OneRTTPacket : Packet {
            };

            constexpr LongPacket* as_long(Packet* p) {
                if (!p) {
                    return nullptr;
                }
                if (p->flags.is_long()) {
                    return static_cast<LongPacket*>(p);
                }
                return nullptr;
            }

            struct InitialPacket : LongPacket {
                tsize token_len;
                byte* token;
            };

            struct HandshakePacket : LongPacket {
            };

            struct ZeroRTTPacket : LongPacket {
            };

            struct RetryPacket : LongPacket {
                tsize retry_token_len;
                byte* retry_token;
                byte integrity_tag[16];
            };

            struct VersionNegotiationPacket : LongPacket {
            };

            template <class Bytes>
            bool read_version(Bytes&& b, uint* version, tsize size, tsize* offset) {
                if (!version || offset == nullptr || *offset + 3 >= size) {
                    return false;
                }
                varint::Swap<uint> v;
                v.swp[0] = b[*offset];
                v.swp[1] = b[*offset + 1];
                v.swp[2] = b[*offset + 2];
                v.swp[3] = b[*offset + 3];
                *version = varint::swap_if(v.t);
                *offset += 4;
                return true;
            }

            template <class T>
            concept as_pointer = std::is_pointer_v<T>;

            template <class T>
            concept PacketReadContext = requires(T t) {
                { t.discard((byte*)nullptr, tsize{}, (tsize*)nullptr, FirstByte{}, uint{}, bool{}) } -> std::same_as<Error>;

                { t.initial((InitialPacket*)nullptr) } -> std::same_as<Error>;

                { t.zero_rtt((ZeroRTTPacket*)nullptr) } -> std::same_as<Error>;

                { t.handshake((HandshakePacket*)nullptr) } -> std::same_as<Error>;

                { t.retry((RetryPacket*)nullptr) } -> std::same_as<Error>;

                { t.one_rtt((OneRTTPacket*)nullptr) } -> std::same_as<Error>;

                {t.packet_error(Error{}, (const char*)nullptr, (Packet*)nullptr)};

                {t.varint_error(varint::Error{}, (const char*)nullptr, (Packet*)nullptr)};

                { t.get_bytes(tsize{}) } -> std::same_as<bytes::Bytes>;

                {t.put_bytes(std::declval<bytes::Bytes&&>())};

                { t.get_issued_connid(byte{}) } -> as_pointer;
            };

            template <class Bytes>
            Error read_packet_number(Bytes&& b, tsize size, tsize* offset, FirstByte& fb, uint* packet_number, auto& discard) {
                byte packet_number_len = (fb.bits & 0x03) + 1;
                if (offset + packet_number_len > size) {
                    return discard(Error::not_enough_length, "read_packet_number/packet_number_len");
                }
                varint::Swap<uint> v{0};
                for (byte i = 0; i < packet_number_len; i++) {
                    v.swp[packet_number_len - 1 - i] = b[*offset];
                    ++*offset;
                }
                *packet_number = varint::swap_if(v.t);
                return Error::none;
            }

            template <class Bytes, class Next>
            Error read_packet_number_long(Bytes&& b, tsize size, tsize* offset, FirstByte* fb, uint* packet_number, Next&& next, auto& discard) {
                Error err = next.unmask_fb(fb);
                if (err != Error::none) {
                    return discard(err, "read_packet_number_long/unmask_fb");
                }
                err = read_packet_number(b, size, offset, fb, packet_number, discard);
                if (err != Error::none) {
                    return err;
                }
                err = next.save_packet_number(packet_number);
                if (err != Error::none) {
                    return discard(err, "read_packet_number_long/save_packet_number");
                }
                return Error::none;
            }

            constexpr auto connIDLenLimit = 20;

#define CHECK_OFFSET(LEN)                    \
    {                                        \
        if (size < LEN + *offset) {          \
            return Error::not_enough_length; \
        }                                    \
    }
#define CHECK_OFFSET_CB(LEN, ...)   \
    {                               \
        if (size < LEN + *offset) { \
            __VA_ARGS__             \
        }                           \
    }

            template <class Bytes, class Next>
            Error read_bytes(Bytes&& b, tsize size, tsize* offset, bytes::Bytes& res, tsize* reslen, Next& next, tsize len) {
                CHECK_OFFSET(len)
                // protect from remaining id
                next.put_bytes(std::move(res));
                *reslen = conn::InvalidLength;
                auto dst = next.get_bytes(len + 1);
                byte* target = dst.own();
                if (!target) {
                    return Error::memory_exhausted;
                }
                bytes::copy(target, b, *offset + len, *offset);
                target[len] = 0;  // null terminated
                res = std::move(dst);
                *reslen = len;
                *offset += len;
                return Error::none;
            }

            template <class Bytes, class Next>
            Error read_connID(Bytes&& b, tsize size, tsize* offset, byte*& id, tsize& len, Next& next) {
                CHECK_OFFSET(1)
                len = b[*offset];
                ++*offset;
                if (len > connIDLenLimit) {
                    return Error::too_long_connection_id;
                }
                CHECK_OFFSET(len)
                id = b + *offset;
                *offset += len;
                return Error::none;
            }

        }  // namespace packet
    }      // namespace quic
}  // namespace utils

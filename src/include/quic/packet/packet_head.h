/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_head - quic head of packet header
#pragma once

#include "../common/variable_int.h"
#include <type_traits>
#include <concepts>

namespace utils {
    namespace quic {
        namespace packet {
            enum class FBInfo {
                should_discard,
                long_header,
                short_header,
            };

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
                unkown_connection_id,
            };

            struct FirstByte {
                FBInfo behave;
                types type;
                byte bits;
                tsize offset;
            };

            constexpr FirstByte guess_packet(byte first_byte) {
                auto fixed_bit = first_byte & 0x40;
                if (fixed_bit == 0) {
                    // if 0x40 bit is not 1
                    // this can be VersionNegotiation packet
                    return {FBInfo::should_discard, types::VersionNegotiation, byte{first_byte & 0x7f}};
                }
                auto header_form = first_byte & 0x80;
                if (header_form == 0) {
                    return {FBInfo::short_header, types::OneRTT, byte{first_byte & 0x3f}};
                }
                byte long_header_type = (first_byte & 0x30) >> 4;
                return {FBInfo::long_header, types{long_header_type}, byte{first_byte & 0x0f}};
            }

            template <class Bytes>
            bool read_version(Bytes&& b, uint* version, tsize size, tsize* offset) {
                if (version || offset == nullptr || *offset + 3 >= size) {
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

            template <class T, class Bytes>
            concept PacketHeadNext = requires(T t) {
                // discard(bytes,size,offset,first_byte,version,versionSuc)
                { t.discard(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), FirstByte{}, uint{}, bool{}) } -> std::same_as<Error>;
                // long_h(bytes,size,offset,first_byte,payload_length)
                { t.long_h(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), FirstByte{}, tsize{}) } -> std::same_as<Error>;
                // short_h(bytes,size,offset,first_byte)
                { t.short_h(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), FirstByte{}) } -> std::same_as<Error>;
                // retry_h(bytes,size,offset,first_byte,payload_length)
                { t.retry_h(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), FirstByte{}, tsize{}) } -> std::same_as<Error>;

                // save_dst(bytes,size,offset,dst_id_length)
                { t.save_dst(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), 0) } -> std::same_as<Error>;
                // save_src(bytes,size,offset,src_id_length)
                { t.save_src(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), 0) } -> std::same_as<Error>;
                { t.save_initial_token(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), 0) } -> std::same_as<Error>;
                { t.unmask_fb(std::declval<FirstByte*>()) } -> std::same_as<Error>;
                { t.save_packet_number(std::declval<tsize*>()) } -> std::same_as<Error>;
                // packet_error(bytes,size,offset,first_byte,version,err,where)
                {t.packet_error(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), FirstByte{}, uint{}, Error{}, std::declval<const char*>())};
                {t.varint_decode_error(std::declval<Bytes>(), tsize{}, std::declval<tsize*>(), FirstByte{}, varint::Error{}, std::declval<const char*>())};
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

        }  // namespace packet
    }      // namespace quic
}  // namespace utils

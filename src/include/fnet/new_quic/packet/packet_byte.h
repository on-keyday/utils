/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <binary/flags.h>
#include "version.h"
#include "packet_type.h"
#include <binary/reader.h>
#include <binary/writer.h>

namespace utils::fnet::quic::packet {

    struct ShortPacketByte {
       private:
        using ShortBits = binary::flags_t<byte, 1, 1, 1, 2, 1, 2>;
        ShortBits short_hdr;

       public:
        constexpr ShortPacketByte(byte data)
            : short_hdr(data) {}

        bits_flag_alias_method(short_hdr, 2, spin_bit);

        bits_flag_alias_method(short_hdr, 3, reserved);

        bits_flag_alias_method(short_hdr, 4, key_phase_bit);

       private:
        bits_flag_alias_method(short_hdr, 5, packet_number_length_raw);

       public:
        constexpr auto packet_number_length() const noexcept {
            return packet_number_length_raw() + 1;
        }

        constexpr bool set_packet_number_length(byte len) noexcept {
            if (len < 1 || 4 < len) {
                return false;
            }
            return set_packet_number_length_raw(len - 1);
        }

        static constexpr byte protect_mask = ShortBits::get_mask<3>() | ShortBits::get_mask<4>() | ShortBits::get_mask<5>();

        constexpr bool valid() const {
            return !short_hdr.get<0>();
        }
    };

    struct LongPacketByte {
       private:
        using LongBits = binary::flags_t<byte, 1, 1, 2, 2, 2>;
        LongBits long_hdr;

       public:
        constexpr LongPacketByte(byte data)
            : long_hdr(data) {}

        bits_flag_alias_method(long_hdr, 2, type_bits);
        bits_flag_alias_method(long_hdr, 3, reserved);

        constexpr packet::Type type(Version version) const noexcept {
            return packet::long_packet_type(type_bits(), version);
        }

        constexpr bool set_type(packet::Type type, Version version) noexcept {
            return set_type_bits(packet::long_packet_type_bit_1_to_3(type, version));
        }

       private:
        bits_flag_alias_method(long_hdr, 4, packet_number_length_raw);

       public:
        // returns 0 if invalid
        constexpr byte packet_number_length(Version v) const noexcept {
            if (type(v) == packet::Type::Retry) {
                return 0;
            }
            return packet_number_length_raw() + 1;
        }

        constexpr bool set_packet_number_length(Version v, byte len) noexcept {
            if (type(v) == packet::Type::Retry) {
                return false;
            }
            if (len < 1 || 4 < len) {
                return false;
            }
            return set_packet_number_length_raw(len - 1);
        }

        static constexpr byte protect_mask = LongBits::get_mask<3>() | LongBits::get_mask<4>();

        constexpr bool valid() const {
            return long_hdr.get<0>();
        }
    };

    struct PacketByte {
       private:
        using CommonBits = binary::flags_t<byte, 1, 1>;
        byte data;

       public:
        constexpr PacketByte(byte data)
            : data(data) {}

        constexpr operator byte() const {
            return data;
        }

        constexpr void set_header_form(bool is_long) {
            auto d = CommonBits(data);
            d.set<0>(is_long);
            data = d.as_value();
        }

        constexpr bool is_long() const {
            return CommonBits(data).get<0>();
        }

        constexpr bool is_short() const {
            return !CommonBits(data).get<0>();
        }

        constexpr bool fixed_bit() const {
            return CommonBits(data).get<1>();
        }

        constexpr byte protect_mask() const {
            return is_long() ? LongPacketByte::protect_mask : ShortPacketByte::protect_mask;
        }

        // returns 0 if invalid
        constexpr byte packet_number_length(Version version) const noexcept {
            if (is_long()) {
                return LongPacketByte(data).packet_number_length(version);
            }
            else {
                return ShortPacketByte(data).packet_number_length();
            }
        }
    };
}  // namespace utils::fnet::quic::packet

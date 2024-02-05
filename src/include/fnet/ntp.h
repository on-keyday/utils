/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/number.h>
#include <binary/flags.h>

namespace futils::fnet::ntp {

    template <class T>
    struct TimeStampFormatBase {
        T seconds = 0;
        T fraction = 0;

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(r, true, seconds, fraction);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return binary::write_num_bulk(w, true, seconds, fraction);
        }
    };

    using ShortFormat = TimeStampFormatBase<std::uint16_t>;
    using TimeStampFormat = TimeStampFormatBase<std::uint32_t>;

    struct DateFormat {
        std::uint32_t era_number = 0;
        std::uint32_t era_offset = 0;
        std::uint64_t fraction = 0;

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(r, true, era_number, era_offset, fraction);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return binary::write_num_bulk(w, true, era_number, era_offset, fraction);
        }
    };

    struct Extention {
        std::uint16_t type = 0;
        std::uint16_t len = 0;
        view::rvec data;

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(r, true, type, len) &&
                   r.read(data, len);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            auto padding = 4 - (data.size() & 0x3);
            if (data.size() + padding > 0xffff) {
                return false;
            }
            return binary::write_num_bulk(w, true, type, std::uint16_t(data.size() + padding)) &&
                   w.write(data) &&
                   w.write(0, padding);
        }
    };

    enum class LeapWarning : byte {
        none = 0,
        last_61 = 1,
        last_59 = 2,
        unknown = 3,
    };

    enum class Mode : byte {
        reserved,
        symmetric_active,
        symmetric_passive,
        client,
        server,
        broadcast,
        ntp_control_message,
        reserved_for_private,
    };

    struct Packet {
       private:
        binary::flags_t<byte, 2, 3, 3> first_byte;

       public:
        bits_flag_alias_method_with_enum(first_byte, 0, leap, LeapWarning);
        bits_flag_alias_method(first_byte, 1, version);
        bits_flag_alias_method_with_enum(first_byte, 2, mode, Mode);

        byte stratum = 0;
        byte poll = 0;
        byte precision = 0;
        std::uint32_t root_delay = 0;
        std::uint32_t root_dispersion = 0;
        std::uint32_t reference_id = 0;
        std::uint64_t reference_timestamp = 0;
        std::uint64_t origin_timestamp = 0;
        std::uint64_t receive_timestamp = 0;
        std::uint64_t transmit_timestamp = 0;
        view::rvec extensions;
        std::uint32_t key_identifier = 0;
        byte digest[16]{0};

        constexpr bool parse(binary::reader& r) {
            return binary::read_num_bulk(
                       r, true,
                       first_byte.as_value(), stratum, poll, precision,
                       root_delay, root_dispersion,
                       reference_id, reference_timestamp,
                       origin_timestamp, receive_timestamp, transmit_timestamp) &&
                   r.read(extensions, r.remain().size() - 4 - 16) &&
                   binary::read_num(r, key_identifier) &&
                   r.read(digest);
        }

        constexpr bool render(binary::writer& w) const {
            return binary::write_num_bulk(
                       w, true,
                       first_byte.as_value(), stratum, poll, precision,
                       root_delay, root_dispersion,
                       reference_id, reference_timestamp,
                       origin_timestamp, receive_timestamp, transmit_timestamp) &&
                   w.write(extensions) &&
                   binary::write_num(w, key_identifier) &&
                   w.write(digest);
        }
    };

}  // namespace futils::fnet::ntp

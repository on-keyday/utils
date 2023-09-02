/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../error.h"
#include <binary/reader.h>
#include <binary/number.h>
#include <binary/flags.h>
#include <cstdint>
#include "protocol.h"

namespace utils::fnet::ip {

    constexpr expected<std::uint16_t> checksum(view::rvec data, std::uint16_t initial = 0, bool fill_zero_if_non_2 = false) {
        if (!fill_zero_if_non_2 && data.size() & 0x1) {
            return unexpect("not 2 byte aligned");
        }
        std::uint32_t sum = initial;
        auto add_sum = [&](std::uint16_t val) {
            sum += val;
            if (sum & 0x10000) {
                sum &= 0xffff;
                sum += 0x1;
            }
        };
        binary::reader r{data};
        while (r.remain().size() >= 8) {
            std::uint16_t v[4]{};
            [&](auto&... arg) {
                binary::read_num_bulk(r, true, arg...);
                (..., add_sum(arg));
            }(v[0], v[1], v[2], v[3]);
        }
        while (r.remain().size() > 1) {
            std::uint16_t val;
            binary::read_num(r, val);
            add_sum(val);
        }
        if (r.remain().size()) {
            add_sum(std::uint16_t(r.top()) << 8);
        }
        return ~std::uint16_t(sum);
    }

    namespace test {
        constexpr bool check_ipv4_checksum() {
            byte pkt[] = {0x45, 0x00, 0x16, 0xce, 0x65, 0x4c, 0x40, 0x00,
                          0x01, 0x11, 0x00, 0x00, 0xc0, 0xa8, 0x65, 0x1f,
                          0xe0, 0x00, 0x00, 0x1f};
            checksum(pkt)
                .and_then([](std::uint16_t val) -> expected<void> {
                    if (val != 0xf7eb) {
                        return unexpect(error::Error(val));
                    }
                    return {};
                })
                .value();
            binary::writer w{pkt};
            w.offset(10);
            binary::write_num(w, std::uint16_t(0xf7eb));
            checksum(pkt)
                .and_then([](std::uint16_t val) -> expected<void> {
                    if (val != 0) {
                        return unexpect(error::Error(val));
                    }
                    return {};
                })
                .value();
            return true;
        }

        static_assert(check_ipv4_checksum());
    }  // namespace test

    struct IPv4Address {
        byte addr[4]{};

        constexpr bool parse(binary::reader& r) noexcept {
            return r.read(addr);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return w.write(addr);
        }
    };

    struct IPv4Header {
       private:
        binary::flags_t<byte, 4, 4> first_byte;
        bits_flag_alias_method(first_byte, 1, ihl);

       public:
        bits_flag_alias_method(first_byte, 0, version);
        std::uint8_t type_of_service = 0;
        std::uint16_t packet_len = 0;  // include header
        std::uint16_t id = 0;
        std::uint16_t frag_off = 0;
        std::uint8_t ttl = 0;
        Protocol protocol = Protocol::hopopt;
        std::uint16_t check_sum = 0;
        IPv4Address src_addr;
        IPv4Address dst_addr;

        constexpr byte header_length() const noexcept {
            return ihl() << 2;
        }

        constexpr bool set_header_length(byte length) noexcept {
            if (length & 0x3) {
                return false;
            }
            return set_ihl(length >> 2);
        }

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(
                       r, true,
                       first_byte.as_value(), type_of_service, packet_len, id, frag_off, ttl, protocol, check_sum) &&
                   src_addr.parse(r) &&
                   dst_addr.parse(r);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return binary::write_num_bulk(
                       w, true,
                       first_byte.as_value(), type_of_service, packet_len, id, frag_off, ttl, protocol, check_sum) &&
                   src_addr.render(w) &&
                   dst_addr.render(w);
        }

        constexpr expected<void> parse_with_checksum(binary::reader& r) noexcept {
            auto cs = r.remain();
            auto cur = r.offset();
            if (!parse(r)) {
                return unexpect("not enough length for IPv4 header");
            }
            auto target = cs.substr(0, r.offset() - cur);
            if (check_sum == 0) {  // validation disabled
                return {};
            }
            return checksum(target)
                .and_then([](std::uint16_t val) -> expected<void> {
                    if (val != 0) {
                        return unexpect("checksum is not zero");
                    }
                    return {};
                });
        }

        constexpr expected<void> render_with_checksum(binary::writer& w) const noexcept {
            auto cs = w.remain();
            auto cur = w.offset();
            auto pkt = *this;   // copy
            pkt.check_sum = 0;  // currently set checksum 0
            if (!pkt.render(w)) {
                return unexpect("not enough length for IPv4 header");
            }
            auto target = cs.substr(0, w.offset() - cur);
            return checksum(target)
                .and_then([&](std::uint16_t val) -> expected<void> {
                    binary::writer w{target};
                    w.offset(10);
                    binary::write_num(w, val);  // fill checksum
                    return {};
                });
        }
    };

    enum class IPOptionType : byte {
        end_of_options,
        nop,
        security = 130,
        loose_source_record_route = 131,
        strict_source_record_route = 132,
        record_route = 7,
        stream_identifier = 136,
        timestamp = 68,
    };

    constexpr bool is_copied(IPOptionType ty) {
        return byte(ty) & 0x80;
    }

    enum class OptionClass : byte {
        control = 0,
        reserved_for_future_use_1 = 1,
        debugging_and_measurement = 2,
        reserved_for_future_use_2 = 3,
    };

    constexpr OptionClass option_class(IPOptionType typ) {
        return OptionClass((byte(typ) >> 5) & 0x2);
    }

    constexpr byte option_number(IPOptionType typ) {
        return byte(typ) & 0x1f;
    }

    struct IPOption {
        IPOptionType type = IPOptionType::end_of_options;
        byte len = 0;
        view::rvec data;

        constexpr bool parse(binary::reader& r) noexcept {
            if (!binary::read_num(r, type)) {
                return false;
            }
            if (type == IPOptionType::end_of_options || type == IPOptionType::nop) {
                return true;  // 1 octet
            }
            return binary::read_num(r, len) &&
                   r.read(data, len);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            if (byte(type) >= 2) {
                if (data.size() > 0xff) {
                    return false;
                }
            }
            if (!w.write(byte(type), 1)) {
                return false;
            }
            if (type == IPOptionType::end_of_options || type == IPOptionType::nop) {
                return true;
            }
            return w.write(byte(data.size()), 1) &&
                   w.write(data);
        }
    };

    struct IPv6Address {
        byte addr[16]{};

        constexpr bool parse(binary::reader& r) noexcept {
            return r.read(addr);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return w.write(addr);
        }
    };

    struct IPv6Header {
       private:
        binary::flags_t<std::uint32_t, 4, 8, 20> first_4byte;

       public:
        bits_flag_alias_method(first_4byte, 0, version);
        bits_flag_alias_method(first_4byte, 1, traffic_class);
        bits_flag_alias_method(first_4byte, 2, flow_label);
        std::uint16_t payload_length = 0;
        Protocol next_hdr = Protocol::hopopt;
        byte hop_limit = 0;
        IPv6Address src_addr;
        IPv6Address dst_addr;

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(r, true, first_4byte.as_value(), next_hdr, hop_limit) &&
                   src_addr.parse(r) &&
                   dst_addr.parse(r);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return binary::write_num_bulk(w, true, first_4byte.as_value(), next_hdr, hop_limit) &&
                   src_addr.render(w) &&
                   dst_addr.render(w);
        }
    };

    struct IPv6Option {
        Protocol next_hdr;
        byte ext_hdr_length;
    };

    struct IPv4PseudoHeader {
        IPv4Address src_address, dst_address;
        Protocol protocol_number = Protocol::udp;
        std::uint16_t payload_length = 0;  // include UDP/TCP header and payload

        constexpr bool parse(binary::reader& r) noexcept {
            byte tmp = 0;
            return src_address.parse(r) && dst_address.parse(r) &&
                   binary::read_num_bulk(r, true, tmp, protocol_number, payload_length);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return src_address.render(w) && dst_address.render(w) &&
                   binary::write_num_bulk(w, true, std::uint8_t(0), protocol_number, payload_length);
        }

        constexpr std::uint16_t checksum() const {
            byte data[4 + 4 + 4];
            binary::writer w{data};
            render(w);
            return ip::checksum(data).value();
        }
    };

    struct IPv6PseudoHeader {
        IPv6Address src_address, dst_address;
        std::uint32_t payload_length = 0;  // include UDP/TCP header and payload
        Protocol next_header = ip::Protocol::udp;

        constexpr bool parse(binary::reader& r) noexcept {
            byte tmp = 0;
            return src_address.parse(r) && dst_address.parse(r) &&
                   binary::read_num_bulk(r, true, payload_length, tmp, tmp, tmp, next_header);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return src_address.render(w) && dst_address.render(w) &&
                   binary::write_num_bulk(w, true, payload_length, std::uint8_t(0), std::uint8_t(0), std::uint8_t(0), next_header);
        }

        constexpr std::uint16_t checksum() const {
            byte data[16 + 16 + 4 + 4];
            binary::writer w{data};
            render(w);
            return ip::checksum(data).value();
        }
    };

    namespace test {
        constexpr bool check_checksum() {
            IPv4PseudoHeader ipv4;
            ipv4.src_address = {{192, 168, 11, 1}};
            ipv4.dst_address = {{239, 0, 0, 1}};
            ipv4.payload_length = 8 + 50;
            ipv4.protocol_number = ip::Protocol::udp;
            auto initial = ipv4.checksum();
            return true;
        }
        static_assert(check_checksum());
    }  // namespace test

}  // namespace utils::fnet::ip

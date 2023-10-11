/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "ip.h"

namespace utils::fnet::icmp {
    enum class Type : byte {
        echo_reply,
        reserved_1,
        reserved_2,
        dst_unreachable,
        src_quench,
        redirect,
        alt_host_address,  // deprecated
        reserved_3,
        echo,
        router_advertisement,
        router_solicitation,
        time_exceeded,
        parameter_problem,
        timestamp,
        timestamp_reply,
        information_request,
        information_reply,
        address_mask_request,
        address_mask_reply,
        reserved_for_security_1,
        reserved_for_robustness_experiment_1,
        reserved_for_robustness_experiment_2,
        reserved_for_robustness_experiment_3,
        reserved_for_robustness_experiment_4,
        reserved_for_robustness_experiment_5,
        reserved_for_robustness_experiment_6,
        reserved_for_robustness_experiment_7,
        reserved_for_robustness_experiment_8,
        reserved_for_robustness_experiment_9,
        reserved_for_robustness_experiment_10,
        // belows are deprecated or experimental
        traceroute,
        datagram_conversion_error,
        mobile_host_redirect,
        where_are_you,
        here_I_am,
        mobile_registration_request,
        mobile_registration_reply,
        domain_name_request,
        domain_name_reply,
        skip_discovery,
        security_failure,
        icmp_for_seamoby,
        // others are reserved
        experimental_1 = 253,
        experimental_2 = 255,
    };

    constexpr const char* to_string(Type type) {
        switch (type) {
            case Type::echo_reply:
                return "ECHO_REPLY";
            case Type::reserved_1:
                return "RESERVED";
            case Type::reserved_2:
                return "RESERVED";
            case Type::dst_unreachable:
                return "DESTINATION_UNREACHABLE";
            case Type::src_quench:
                return "SOURCE_QUENCH";
            case Type::redirect:
                return "REDIRECT";
            case Type::alt_host_address:
                return "ALTERNATE_HOST_ADDRESS";
            case Type::reserved_3:
                return "RESERVED";
            case Type::echo:
                return "ECHO";
            case Type::router_advertisement:
                return "ROUTER_ADVERTISEMENT";
            case Type::router_solicitation:
                return "ROUTER_SOLICITATION";
            case Type::time_exceeded:
                return "TIME_EXCEEDED";
            case Type::parameter_problem:
                return "PARAMETER_PROBLEM";
            case Type::timestamp:
                return "TIMESTAMP";
            case Type::timestamp_reply:
                return "TIMESTAMP_REPLY";
            case Type::information_request:
                return "INFORMATION_REQUEST";
            case Type::information_reply:
                return "INFORMATION_REPLY";
            case Type::address_mask_request:
                return "ADDRESS_MASK_REQUEST";
            case Type::address_mask_reply:
                return "ADDRESS_MASK_REPLY";
            case Type::reserved_for_security_1:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_1:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_2:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_3:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_4:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_5:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_6:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_7:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_8:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_9:
                return "RESERVED";
            case Type::reserved_for_robustness_experiment_10:
                return "RESERVED";
            case Type::traceroute:
                return "TRACEROUTE";
            case Type::datagram_conversion_error:
                return "DATAGRAM_CONVERSION_ERROR";
            case Type::mobile_host_redirect:
                return "MOBILE_HOST_REDIRECT";
            case Type::where_are_you:
                return "WHERE_ARE_YOU";
            case Type::here_I_am:
                return "HERE_I_AM";
            case Type::mobile_registration_request:
                return "MOBILE_REGISTRATION_REQUEST";
            case Type::mobile_registration_reply:
                return "MOBILE_REGISTRATION_REPLY";
            case Type::domain_name_request:
                return "DOMAIN_NAME_REQUEST";
            case Type::domain_name_reply:
                return "DOMAIN_NAME_REPLY";
            case Type::skip_discovery:
                return "SKIP_DISCOVERY";
            case Type::security_failure:
                return "SECURITY_FAILURE";
            case Type::icmp_for_seamoby:
                return "ICMP_FOR_SEAMOBY";
            case Type::experimental_1:
                return "EXPERIMENTAL_1";
            case Type::experimental_2:
                return "EXPERIMENTAL_2";
        }
        return "RESERVED";
    }

    constexpr const char* to_string(Type type, byte code) {
        switch (type) {
            case Type::dst_unreachable:
                switch (code) {
                    case 0:
                        return "Destination network unreachable";
                    case 1:
                        return "Destination host unreachable";
                    case 2:
                        return "Destination protocol unreachable";
                    case 3:
                        return "Destination port unreachable";
                    case 4:
                        return "Fragmentation required, and DF flag set";
                    case 5:
                        return "Source route failed";
                    case 6:
                        return "Destination network unknown";
                    case 7:
                        return "Destination host unknown";
                    case 8:
                        return "Source host isolated";
                    case 9:
                        return "Network administratively prohibited";
                    case 10:
                        return "Host administratively prohibited";
                    case 11:
                        return "Network unreachable for TOS";
                    case 12:
                        return "Host unreachable for TOS";
                    case 13:
                        return "Communication administratively prohibited";
                    case 14:
                        return "Host Precedence Violation";
                    case 15:
                        return "Precedence cutoff in effect";
                    default:
                        return "UNKNOWN";
                }
            case Type::redirect: {
                switch (code) {
                    case 0:
                        return "Redirect Datagram for the Network";
                    case 1:
                        return "Redirect Datagram for the Host";
                    case 2:
                        return "Redirect Datagram for the TOS & network";
                    case 3:
                        return "Redirect Datagram for the TOS & host";
                    default:
                        return "UNKNOWN";
                }
            }
            case Type::parameter_problem: {
                switch (code) {
                    case 0:
                        return "Pointer indicates the error";
                    case 1:
                        return "Missing a required option";
                    case 2:
                        return "Bad length";
                    default:
                        return "UNKNOWN";
                }
            }
            case Type::time_exceeded: {
                switch (code) {
                    case 0:
                        return "TTL expired in transit";
                    case 1:
                        return "Fragment reassembly time exceeded";
                    default:
                        return "UNKNOWN";
                }
            }
            default:
                return "";  // no message to add
        }
    }

    constexpr expected<void> render_icmp_with_checksum(binary::writer& w, auto&& render) noexcept {
        auto cur = w.offset();
        if (!render(w)) {
            return unexpect("cannot render; maybe too short buffer", error::Category::lib, error::fnet_icmp_error);
        }
        if (cur >= w.offset()) {
            return unexpect("cannot make offset backward", error::Category::lib, error::fnet_icmp_error);
        }
        size_t size = w.offset() - cur;
        if (size < 4) {
            return unexpect("too short icmp header", error::Category::lib, error::fnet_icmp_error);
        }
        auto data = w.written().substr(cur, size);
        if (data[2] != 0 || data[3] != 0) {
            return unexpect("check sum field is not zero", error::Category::lib, error::fnet_icmp_error);
        }
        return ip::checksum(data, 0, true).transform([&](std::uint16_t v) {
            binary::writer w{data};
            w.offset(2);
            binary::write_num(w, v);
        });
    }

    constexpr expected<void> parse_icmp_with_checksum(binary::reader& r, auto&& parse) noexcept {
        auto cur = r.offset();
        if (!parse(r)) {
            return unexpect("cannot render; maybe too short buffer", error::Category::lib, error::fnet_icmp_error);
        }
        if (cur >= r.offset()) {
            return unexpect("cannot make offset backward", error::Category::lib, error::fnet_icmp_error);
        }
        size_t size = r.offset() - cur;
        if (size < 4) {
            return unexpect("too short icmp header", error::Category::lib, error::fnet_icmp_error);
        }
        auto data = r.read().substr(cur, size);
        return ip::checksum(data, 0, true).and_then([&](std::uint16_t v) -> expected<void> {
            if (v != 0) {
                return unexpect("checksum failure", error::Category::lib, error::fnet_icmp_error);
            }
            return {};
        });
    }

    struct ICMPHeader {
        Type type = Type::echo_reply;
        byte code = 0;
        std::uint16_t check_sum = 0;

        constexpr bool parse(binary::reader& r) noexcept {
            return binary::read_num_bulk(r, true, type, code, check_sum);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            return binary::write_num_bulk(w, true, type, code, check_sum);
        }
    };

    struct ICMPEcho : ICMPHeader {
        std::uint16_t identifier = 0;
        std::uint16_t seq_number = 0;
        view::rvec data;

        constexpr bool parse(binary::reader& r) noexcept {
            return ICMPHeader::parse(r) &&
                   (type == Type::echo || type == Type::echo_reply) &&
                   binary::read_num(r, identifier) &&
                   binary::read_num(r, seq_number) &&
                   r.read(data, r.remain().size());
        }

        constexpr expected<void> parse_with_checksum(binary::reader& r) noexcept {
            return parse_icmp_with_checksum(r, [&](binary::reader& r) {
                return parse(r);
            });
        }

        constexpr bool render(binary::writer& w) const noexcept {
            if (type != Type::echo && type != Type::echo_reply) {
                return false;
            }
            return ICMPHeader::render(w) &&
                   binary::write_num(w, identifier) &&
                   binary::write_num(w, seq_number) &&
                   w.write(data);
        }

        constexpr expected<void> render_with_checksum(binary::writer& w) const noexcept {
            return render_icmp_with_checksum(w, [&](binary::writer& w) {
                return render(w);
            });
        }
    };

    struct ICMPDatagram {
        view::rvec data;

        constexpr bool parse(binary::reader& r) noexcept {
            std::uint32_t len_pad = 0;
            return binary::read_num(r, len_pad) &&
                   r.read(data, (len_pad & 0x00ff0000) >> 16);
        }

        constexpr bool render(binary::writer& w) const noexcept {
            if (data.size() > 0xff) {
                return false;
            }
            return binary::write_num(w, std::uint32_t(data.size()) << 16) &&
                   w.write(data);
        }
    };
}  // namespace utils::fnet::icmp

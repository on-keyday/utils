/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// socks - socks5 protocol
#pragma once
#include <cstdint>
#include "../core/byte.h"

namespace utils {
    namespace dnet {
        namespace socks {

            struct TCPMethodList {
                byte version;
                byte nmethod;
                byte methods[255];
            };

            bool parse_tcp_method_select(const TCPMethodList& list, auto&& input, size_t len, size_t& red) {
                if (len <= red || len - red < 2) {
                    return false;
                }
                list.version = byte(input[red + 0]);
                list.nmethod = byte(input[red + 1]);
                if (len < red + 2 + list.nmethod) {
                    return false;
                }
                for (auto i = 0; i < list.nmethod; i++) {
                    list.methods[i] = byte(input[red + 2 + i]);
                }
                red += 2 + list.nmethod;
                return true;
            }

            bool render_tcp_method_select(const TCPMethodList& list, auto&& output) {
                output.push_back(list.version);
                output.push_back(list.nmethod);
                for (auto i = 0; i < list.nmethod; i++) {
                    output.push_back(list.methods[i]);
                }
            }

            struct TCPSelected {
                byte version;
                byte method;
            };

            bool parse_tcp_method_reply(TCPSelected& sel, auto&& input, size_t len, size_t& red) {
                if (len <= red || len - red < 2) {
                    return false;
                }
                sel.version = input[red + 0];
                sel.method = input[red + 1];
                red += 2;
                return true;
            }

            void render_tcp_method_reply(TCPSelected& sel, auto&& output) {
                output.push_back(sel.version);
                output.push_back(sel.method);
            }

            struct TCPNegotiation {
                byte version;
                union {
                    byte command;
                    byte reply;
                };
                byte reserved;
                byte address_type;
                byte address[256];
                std::uint16_t port;
            };

            int get_varaddrlen(byte address_type, auto&& input, size_t len, size_t red) {
                int addrlen = 0;
                if (address_type == 0x1) {
                    addrlen = 4;
                }
                else if (address_type == 0x3) {
                    if (len <= red) {
                        return 0;
                    }
                    addrlen = byte(input[red]);
                    addrlen += 1;
                }
                else if (address_type == 0x4) {
                    addrlen = 16;
                }
                else {
                    return -1;
                }
                return addrlen;
            }

            int read_address_and_port(byte address_type, auto&& address, std::uint16_t& port, auto&& input, size_t len, size_t red) {
                int addrlen = get_varaddrlen(req.address_type, input, len, red + 4);
                if (addrlen <= 0) {
                    return addrlen;
                }
                if (len < red + addrlen + 2) {
                    return 0;
                }
                for (auto i = 0; i < addrlen; i++) {
                    address[i] = byte(input[red + i]);
                }
                req.port = (std::uint16_t(input[red + addrlen + 0]) << 8) | std::uint16_t(input[red + addrlen + 1]);
                return addrlen + 2;
            }

            int parse_tcp_negotiation(TCPNegotiation& req, auto&& input, size_t len, size_t& red) {
                if (len <= red || len - red < 4) {
                    return 0;
                }
                req.version = byte(input[red + 0]);
                req.command = byte(input[red + 1]);
                req.reserved = byte(input[red + 2]);
                req.address_type = byte(input[red + 3]);
                int res = read_address_and_port(req.address_type, req.address, req.port, input, len, red + 4);
                if (res <= 0) {
                    return res;
                }
                red += 4 + res;
                return 1;
            }

            bool render_address_and_port(byte address_type, auto&& address, std::uint16_t port, auto&& output) {
                if (address_type == 0x1) {
                    output.push_back(address[0]);
                    output.push_back(address[1]);
                    output.push_back(address[2]);
                    output.push_back(address[3]);
                }
                else if (address_type == 0x3) {
                    auto len = address[0];
                    for (auto i = 0; i < len; i++) {
                        output.push_back(address[i + 1]);
                    }
                }
                else if (address_type == 0x4) {
                    output.push_back(address[0]);
                    output.push_back(address[1]);
                    output.push_back(address[2]);
                    output.push_back(address[3]);
                    output.push_back(address[4]);
                    output.push_back(address[5]);
                    output.push_back(address[6]);
                    output.push_back(address[7]);
                    output.push_back(address[8]);
                    output.push_back(address[9]);
                    output.push_back(address[10]);
                    output.push_back(address[11]);
                    output.push_back(address[12]);
                    output.push_back(address[13]);
                    output.push_back(address[14]);
                    output.push_back(address[15]);
                }
                else {
                    return false;
                }
                output.push_back(byte((port >> 8) & 0xff));
                output.push_back(byte((port)&0xff));
                return true;
            }

            bool render_tcp_negotiation(const TCPNegotiation& req, auto&& output) {
                output.push_back(req.version);
                output.push_back(req.command);
                output.push_back(req.reserved);
                output.push_back(req.address_type);
                return render_address_and_port(req.address_type, req.address, req.port);
            }

            struct UDPRequest {
                std::uint16_t reserved;
                byte fragment_number;
                byte address_type;
                byte address[256];
                std::uint16_t port;
            };

            int parse_udp_request(UDPRequest& req, auto&& input, size_t len, size_t& red) {
                if (len <= red || len - red < 4) {
                    return 0;
                }
                req.reserved = std::uint16_t(byte(input[red + 0])) << 8 | byte(input[red + 1]);
                req.fragment_number = input[red + 2];
                req.address_type = input[red + 3];
                auto res = read_address_and_port(req.address_type, req.address, req.port, input, len, red + 4);
                if (res <= 0) {
                    return res;
                }
                red += 4 + res;
                return 1;
            }

            struct UserPassword {
                byte version;
                byte user_length;
                byte user[255];
                byte password_length;
                byte password[255];
            };

            bool parse_user_password(UserPassword& user, auto&& input, size_t len, size_t& red) {
                if (len <= red || len - red < 2) {
                    return false;
                }
                user.version = input[red + 0];
                user.user_length = input[red + 1];
                if (len < red + 2 + user.user_length + 1) {
                    return false;
                }
                for (auto i = 0; i < user.user_length; i++) {
                    user.user[i] = byte(input[red + 2 + i]);
                }
                user.password_length = byte(input[red + 2 + user.user_length]);
                if (len < red + 2 + user.user_length + 1 + user.password_length) {
                    return false;
                }
                for (auto i = 0; i < user.password_length; i++) {
                    user.password[i] = byte(input[red + 2 + user.user_length + 1 + i]);
                }
                red += 2 + user.user_length + 1 + user.password_length;
                return true;
            }

            void render_user_password(UserPassword& user, auto&& output) {
                output.push_back(user.version);
                output.push_back(user.user_length);
                for (auto i = 0; i < user.user_length; i++) {
                    output.push_back(user.user[i]);
                }
                output.push_back(user.password_length);
                for (auto i = 0; i < user.password_length; i++) {
                    output.push_back(user.password[i]);
                }
            }
        }  // namespace socks
    }      // namespace dnet
}  // namespace utils

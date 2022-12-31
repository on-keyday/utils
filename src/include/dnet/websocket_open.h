/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// websocket_open - websocket opening protocol
#pragma once
#include "websocket.h"
#include "http.h"
#include "../net_util/sha1.h"
#include "../net_util/base64.h"

namespace utils {
    namespace dnet {
        constexpr const char* websocket_magic_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        namespace websocket {
            enum Error {
                error_none,
                error_base64,
                error_sha1,
                error_http,
                error_http_method,
                error_upgrade_missing,
                error_connection_missing,
                error_version_missing,
                error_sec_websocket_accept_missing,
                error_invalid_sec_websocket_key,
                error_http_status,
            };

            using ClientKey = number::Array<char, 26 + utils::strlen(websocket_magic_guid), true>;
            using SHAHash = number::Array<char, 20>;

            Error open(HTTP& http, auto&& path, auto&& header, auto& compare_sec_key) {
                std::random_device dev;
                std::uniform_int_distribution uf(0, 0xff);
                byte data[16]{};
                for (auto& v : data) {
                    v = uf(dev);
                }
                ClientKey fixed;
                SHAHash hash;
                if (!net::base64::encode(view::CharVec(data, sizeof(data)), fixed)) {
                    return error_base64;
                }
                header.emplace("Sec-WebSocket-Key", fixed.c_str());
                header.emplace("Upgrade", "websocket");
                header.emplace("Connection", "Upgrade");
                header.emplace("Sec-WebSocket-Version", "13");
                helper::append(fixed, websocket_magic_guid);
                if (!net::sha::make_sha1(fixed, hash)) {
                    return error_sha1;
                }
                if (!net::base64::encode(hash, compare_sec_key)) {
                    return error_base64;
                }
                if (!http.write_request("GET", path, header)) {
                    return error_http;
                }
                return error_none;
            }

            template <class TmpString = String>
            Error accept(HTTP& http, auto&& path, auto&& header, auto&& pass_sec_key, size_t* red = nullptr) {
                number::Array<char, 4, true> meth;
                HTTPBodyInfo body;
                ClientKey fixed;
                bool version = false;
                bool upgrade = false;
                bool connection = false;
                auto res = http.read_request<TmpString>(meth, path, header, &body, true, helper::nop, [&](auto&& key, auto&& value) {
                    if (helper::equal(key, "Sec-WebSocket-Key", helper::ignore_case())) {
                        helper::append(fixed, value);
                    }
                    else if (helper::equal(key, "Sec-Websocket-Version", helper::ignore_case())) {
                        version = true;  // not check value
                    }
                    else if (helper::equal(key, "Connection", helper::ignore_case())) {
                        connection = helper::contains(value, "Upgrade", helper::ignore_case());
                    }
                    else if (helper::equal(key, "Upgrade", helper::ignore_case())) {
                        upgrade = helper::contains(value, "websocket", helper::ignore_case());
                    }
                });
                if (res == 0) {
                    return error_http;
                }
                if (red) {
                    *red = res;
                }
                if (!version) {
                    return error_version_missing;
                }
                if (!connection) {
                    return error_connection_missing;
                }
                if (!upgrade) {
                    return error_upgrade_missing;
                }
                if (meth.size() != 3 || meth[0] != 'G' || meth[1] != 'E' || meth[2] != 'T') {
                    return error_http_method;
                }
                if (fixed.size() == 0 || fixed.size() > 26) {
                    return error_invalid_sec_websocket_key;
                }
                helper::append(fixed, websocket_magic_guid);
                SHAHash hash;
                if (!net::sha::make_sha1(fixed, hash)) {
                    return error_sha1;
                }
                if (!net::base64::encode(hash, pass_sec_key)) {
                    return error_base64;
                }
                return error_none;
            }

            Error switch_protocol(HTTP& http, auto&& header, auto&& pass_sec_key) {
                header.emplace("Upgrade", "websocket");
                header.emplace("Connection", "Upgrade");
                header.emplace("Sec-WebSocket-Accept", pass_sec_key);
                if (!http.write_response(101, header)) {
                    return error_http;
                }
                return error_none;
            }

            template <class TmpString = String>
            Error verify(HTTP& http, auto&& header, auto&& compare_sec_key, size_t* red = nullptr) {
                number::Array<char, 4, true> meth;
                HTTPBodyInfo body;
                ClientKey fixed;
                bool accept = false;
                bool upgrade = false;
                bool connection = false;
                net::h1header::StatusCode code;
                auto res = http.read_response<TmpString>(code, header, &body, true, helper::nop, helper::nop, [&](auto&& key, auto&& value) {
                    if (helper::equal(key, "Sec-WebSocket-Accept", helper::ignore_case())) {
                        accept = helper::equal(value, compare_sec_key);
                    }
                    else if (helper::equal(key, "Connection", helper::ignore_case())) {
                        connection = helper::contains(value, "Upgrade", helper::ignore_case());
                    }
                    else if (helper::equal(key, "Upgrade", helper::ignore_case())) {
                        upgrade = helper::contains(value, "websocket", helper::ignore_case());
                    }
                });
                if (res == 0) {
                    return error_http;
                }
                if (red) {
                    *red = res;
                }
                if (code != 101) {
                    return error_http_status;
                }
                if (!accept) {
                    return error_sec_websocket_accept_missing;
                }
                if (!connection) {
                    return error_connection_missing;
                }
                if (!upgrade) {
                    return error_upgrade_missing;
                }
                return error_none;
            }
        }  // namespace websocket

    }  // namespace dnet
}  // namespace utils

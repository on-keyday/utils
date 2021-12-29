/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// url_request - http request by url
#pragma once

#include "http1.h"
#include "../util/uri.h"
#include "../../wrap/lite/enum.h"

namespace utils {
    namespace net {
        enum class DefaultPath : std::uint8_t {
            root,
            robots_txt,
            index_html,
            index_htm,
            wildcard,
        };
        BEGIN_ENUM_STRING_MSG(DefaultPath, default_path)
        ENUM_STRING_MSG(DefaultPath::robots_txt, "/robots.txt")
        ENUM_STRING_MSG(DefaultPath::index_html, "/index.html")
        ENUM_STRING_MSG(DefaultPath::index_htm, "/index.htm")
        ENUM_STRING_MSG(DefaultPath::wildcard, "*")
        END_ENUM_STRING_MSG("/")

        enum class RequestFlag : std::uint8_t {
            none,
            url_encoded = 0x1,
        };

        struct RequestOption {
            DefaultPath path = DefaultPath::root;
            RequestFlag flag = RequestFlag::none;
        };

        HttpResponse request(const URI& uri, const char* cacert = nullptr, RequestOption opt = {});
        template <class String>
        HttpResponse request(const String& uri, const char* cacert = nullptr, RequestOption opt = {}) {
            URI parsed;
            rough_uri_parse(uri, parsed);
            return request(uri, cacert);
        }

    }  // namespace net
}  // namespace utils
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

namespace utils {
    namespace net {
        HttpResponse request(const URI& uri, const char* cacert = nullptr);
        template <class String>
        HttpResponse request(const String& uri, const char* cacert = nullptr) {
            URI parsed;
            rough_uri_parse(uri, parsed);
            return request(uri, cacert);
        }

    }  // namespace net
}  // namespace utils
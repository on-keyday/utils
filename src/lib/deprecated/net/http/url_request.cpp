/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/http/url_request.h"
#include "../../../include/net/dns/dns.h"

namespace utils {
    namespace net {
        HttpResponse request(const URI& uri, const char* cacert, RequestOption opt) {
            if (uri.scheme != "http" && uri.scheme != "https") {
                return {};
            }
            if (!uri.has_dobule_slash) {
                return {};
            }
            if (!uri.host.size()) {
                return {};
            }
            wrap::string path;
            if (uri.path.size()) {
                path = uri.path;
            }
            else {
                path = default_path(opt.path);
            }
        }
    }  // namespace net
}  // namespace utils

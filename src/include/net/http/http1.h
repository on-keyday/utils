/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// http1 - http1.1 protocol
#pragma once

#include "../../wrap/lite/smart_ptr.h"
#include "../generate/iocloser.h"

namespace utils {
    namespace net {
        namespace internal {
            struct HeaderImpl;
            struct HttpResponseImpl;
        }  // namespace internal

        struct HttpResponse {
            constexpr HttpResponse() {}

           private:
            internal::HttpResponseImpl* impl = nullptr;
        };

        struct Header {
            friend HttpResponse request(IOClose&& io, const char* method, const char* path, Header&& header);
            Header();
            ~Header();

           private:
            internal::HeaderImpl* impl = nullptr;
        };

        HttpResponse request(IOClose&& io, const char* method, const char* path, Header&& header);
    }  // namespace net
}  // namespace utils

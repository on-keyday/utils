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
        }

        struct Header {
            Header();

           private:
            internal::HeaderImpl* impl = nullptr;
        };

        struct HttpResponse {
        };

        HttpResponse request(IOClose& io, const char* method, Header& header);
    }  // namespace net
}  // namespace utils

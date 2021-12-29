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

        struct Header;

        struct HttpResponse {
            friend HttpResponse request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header);
            friend HttpResponse request(HttpResponse&& io, const char* method, const char* path, Header&& header);
            constexpr HttpResponse() {}
            HttpResponse(HttpResponse&&);
            HttpResponse& operator=(HttpResponse&&);

            Header get_response();

            bool failed() const;

           private:
            internal::HttpResponseImpl* impl = nullptr;
        };

        struct Header {
            friend struct HttpResponse;

           private:
            constexpr Header(std::nullptr_t) {}

           public:
            friend HttpResponse request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header);
            friend HttpResponse request(HttpResponse&& io, const char* method, const char* path, Header&& header);
            Header();
            ~Header();

            Header(Header&&);
            Header& operator=(Header&&);

            void set(const char* key, const char* value);

            std::uint16_t status() const;

            const char* body(size_t* p = nullptr) const;

            explicit operator bool() const {
                return impl != nullptr;
            }

           private:
            internal::HeaderImpl* impl = nullptr;
        };

        HttpResponse request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header);
        HttpResponse request(HttpResponse&& io, const char* method, const char* path, Header&& header);
    }  // namespace net
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// http1 - http1.1 protocol
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "../../wrap/lite/smart_ptr.h"
#include "../generate/iocloser.h"
#include "../../wrap/lite/enum.h"

namespace utils {
    namespace net {
        namespace http {
            namespace internal {
                struct HeaderImpl;
                struct HttpResponseImpl;
                struct HttpAsyncResponseImpl;
                struct HttpSet;
            }  // namespace internal

            struct Header;
            struct HttpAsyncResult;

            struct DLL HttpResponse {
                friend DLL HttpResponse STDCALL request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header);
                friend DLL HttpResponse STDCALL request(HttpResponse&& io, const char* method, const char* path, Header&& header);
                constexpr HttpResponse() {}
                HttpResponse(HttpResponse&&);
                HttpResponse& operator=(HttpResponse&&);

                Header get_response();

                bool failed() const;

               private:
                internal::HttpResponseImpl* impl = nullptr;
            };

            struct DLL HttpAsyncResponse {
                friend struct internal::HttpSet;
                AsyncIOClose get_io();

                Header response();

                constexpr HttpAsyncResponse() {}

                constexpr HttpAsyncResponse(HttpAsyncResponse&& in)
                    : impl(std::exchange(in.impl, nullptr)) {}
                HttpAsyncResponse& operator=(HttpAsyncResponse&& in);

               private:
                internal::HttpAsyncResponseImpl* impl = nullptr;
            };

            enum class HttpError {
                none,
                transport,
                invalid_arg,
                invalid_header,
                invalid_body,
                read_body,
                read_header,
                write_request,
            };

            BEGIN_ENUM_STRING_MSG(HttpError, error_msg)
            ENUM_STRING_MSG(HttpError::transport, "connection level error")
            ENUM_STRING_MSG(HttpError::invalid_arg, "invalid argument")
            ENUM_STRING_MSG(HttpError::invalid_header, "failed to parse header")
            ENUM_STRING_MSG(HttpError::invalid_body, "failed to parse body")
            ENUM_STRING_MSG(HttpError::read_body, "failed to read body")
            ENUM_STRING_MSG(HttpError::read_header, "failed to read header")
            ENUM_STRING_MSG(HttpError::write_request, "failed to write request")
            END_ENUM_STRING_MSG(nullptr)

            struct HttpAsyncResult {
                HttpAsyncResponse resp;
                HttpError err;
                int base_err;
            };

            struct DLL Header {
                friend struct DLL HttpResponse;
                friend struct internal::HttpSet;
                friend struct DLL HttpAsyncResponse;

               private:
                constexpr Header(std::nullptr_t) {}

               public:
                friend DLL HttpResponse STDCALL request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header);
                friend DLL HttpResponse STDCALL request(HttpResponse&& io, const char* method, const char* path, Header&& header);
                friend DLL async::Future<HttpAsyncResult> STDCALL request_async(AsyncIOClose&& io, const char* host, const char* method, const char* path, Header&& header);
                Header();

                Header& set(const char* key, const char* value);

                std::uint16_t status() const;

                const char* value(const char* key, size_t index = 0);

                const char* response(size_t* p = nullptr);

                const char* body(size_t* p = nullptr) const;

                explicit operator bool() const {
                    return impl != nullptr;
                }

               private:
                wrap::shared_ptr<internal::HeaderImpl> impl = nullptr;
            };

            DLL HttpResponse STDCALL request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header);
            DLL HttpResponse STDCALL request(HttpResponse&& io, const char* method, const char* path, Header&& header);
            DLL async::Future<HttpAsyncResult> STDCALL request_async(AsyncIOClose&& io, const char* host, const char* method, const char* path, Header&& header);

            inline bool is_redirect_range(std::uint16_t status) {
                return status >= 300 && status < 400;
            }
        }  // namespace http
    }      // namespace net
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// request - http2 request
#pragma once
#include <platform/windows/dllexport_header.h>
#include "conn.h"
#include "stream.h"
#include "../http/http1.h"
#include <wrap/light/string.h>

namespace utils {
    namespace net {
        namespace http2 {
            struct H2Response {
                UpdateResult err;
                http::HttpAsyncResponse resp;
            };

            DLL async::Future<H2Response> STDCALL request(wrap::shared_ptr<Context> ctx, http::Header&& h, const wrap::string& data = wrap::string{});
            DLL H2Response STDCALL request(async::Context& ctx, wrap::shared_ptr<Context> h2ctx, http::Header&& h, const wrap::string& data = wrap::string{});
        }  // namespace http2
    }      // namespace net
}  // namespace utils

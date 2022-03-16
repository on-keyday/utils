/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// request - http2 request
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "conn.h"
#include "stream.h"
#include "../http/http1.h"

namespace utils {
    namespace net {
        namespace http2 {
            struct H2Result {
                H2Error err = H2Error::none;
                wrap::shared_ptr<Frame> frame;
            };

            DLL async::Future<http::HttpAsyncResponse> STDCALL request(wrap::shared_ptr<Context> ctx, http::Header&& h, const wrap::string& data = wrap::string{});
        }  // namespace http2
    }      // namespace net
}  // namespace utils

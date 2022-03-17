/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// request_methods - request method suite
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "stream.h"

namespace utils {
    namespace net {
        namespace http2 {
            DLL ReadResult STDCALL default_handle_ping_and_data(async::Context& ctx, wrap::shared_ptr<Context>& h2ctx);
            DLL UpdateResult STDCALL handle_ping(async::Context& ctx, wrap::shared_ptr<Context>& h2ctx, Frame& frame);
            DLL ReadResult STDCALL wait_data_async(async::Context& ctx, wrap::shared_ptr<Context>& h2ctx, std::int32_t id, wrap::string* ptr, bool end_stream);
            DLL UpdateResult STDCALL update_window_async(async::Context& ctx, wrap::shared_ptr<Context>& h2ctx, std::int32_t id, std::uint32_t incr);
            DLL UpdateResult STDCALL send_header_async(async::Context& ctx, wrap::shared_ptr<Context>& h2ctx, http::Header h, bool end_stream);
        }  // namespace http2
    }      // namespace net
}  // namespace utils

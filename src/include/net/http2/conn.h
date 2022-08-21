/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// conn - http protocol version 2 connetion
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "../../wrap/light/smart_ptr.h"
#include "frame.h"
#include "../../async/worker.h"
#include "../generate/iocloser.h"
#include "../../wrap/light/string.h"

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {
                struct Http2Impl;
            }

            struct OpenResult;

            struct DLL Conn {
                friend DLL OpenResult STDCALL open_async(async::Context&, AsyncIOClose&& io);
                async::Future<wrap::shared_ptr<Frame>> read();
                async::Future<bool> write(const Frame& frame);
                wrap::shared_ptr<Frame> read(async::Context& ctx);
                bool write(async::Context& ctx, const Frame& frame);
                bool serialize_frame(IBuffer& buf, const Frame& frame);
                WriteInfo write_serial(async::Context& ctx, const wrap::string& buf);
                State close(bool force = false);

                AsyncIOClose get_io();

                H2Error get_error();

                void set_error(H2Error err);

                int get_errorcode();

                ~Conn();

               private:
                wrap::shared_ptr<internal::Http2Impl> impl;
            };

            struct OpenResult {
                wrap::shared_ptr<Conn> conn;
                int errcode;
            };

            DLL async::Future<OpenResult> STDCALL open_async(AsyncIOClose&& io);
            DLL OpenResult STDCALL open_async(async::Context& ctx, AsyncIOClose&& io);
        }  // namespace http2
    }      // namespace net
}  // namespace utils

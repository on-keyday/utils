/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// conn - http protocol version 2 connetion
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "../../wrap/lite/smart_ptr.h"
#include "frame.h"
#include "../../async/worker.h"
#include "../generate/iocloser.h"

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {
                struct Http2Impl;
            }
            struct DLL Conn {
                friend DLL async::Future<wrap::shared_ptr<Conn>> STDCALL open_async(AsyncIOClose&& io);
                async::Future<wrap::shared_ptr<Frame>> read();
                async::Future<bool> write(const Frame& frame);
                State close(bool force = false);

                AsyncIOClose get_io();

                H2Error get_error();

                int get_errorcode();

                ~Conn();

               private:
                wrap::shared_ptr<internal::Http2Impl> impl;
            };

            DLL async::Future<wrap::shared_ptr<Conn>> STDCALL open_async(AsyncIOClose&& io);
        }  // namespace http2
    }      // namespace net
}  // namespace utils

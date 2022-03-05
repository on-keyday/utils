/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// conn - http protocol version 2 connetion
#pragma once
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
            struct Conn {
                async::Future<wrap::shared_ptr<Frame>> read();
                bool write(wrap::shared_ptr<Frame> frame);

               private:
                wrap::shared_ptr<internal::Http2Impl> impl;
            };
        }  // namespace http2
    }      // namespace net
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// http2 - http2 interface
#pragma once
#include "cnet.h"
#include "../net/http2/frame.h"

namespace utils {
    namespace cnet {
        namespace http2 {

            DLL CNet* STDCALL create_client();

            struct Frames;

            wrap::shared_ptr<net::http2::Frame>* begin(Frames*);

            wrap::shared_ptr<net::http2::Frame>* end(Frames*);

        }  // namespace http2
    }      // namespace cnet
}  // namespace utils

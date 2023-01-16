/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// async_resp_impl - response impl
#pragma once
#include "header_impl.h"
#include <net_util/http/body.h>

namespace utils {
    namespace net {
        namespace http {
            namespace internal {
                struct HttpAsyncResponseImpl {
                    HeaderImpl* header = nullptr;
                    AsyncIOClose io;
                    wrap::string reqests;
                    h1body::BodyType bodytype = h1body::BodyType::no_info;
                    size_t expect = 0;
                    Header response;
                    wrap::string hostname;
                };
            }  // namespace internal
        }      // namespace http
    }          // namespace net
}  // namespace utils

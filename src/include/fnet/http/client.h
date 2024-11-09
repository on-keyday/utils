/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "http.h"
#include "../util/uri.h"

namespace futils::fnet::http {

    struct Response {
       private:
       public:
    };

    struct Client {
       private:
        struct AbstractClient;

        std::shared_ptr<AbstractClient> client;

       public:
        std::optional<Response> get_response();
    };

}  // namespace futils::fnet::http

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../storage.h"

namespace utils {
    namespace fnet::quic::token {

        struct RetryToken {
           private:
            storage token;

           public:
            void on_retry_received(view::rvec retry_token) {
                token = make_storage(retry_token);
            }

            constexpr view::rvec get_token() const {
                return token;
            }
        };

        struct Token {
            view::rvec token;
        };

        struct TokenStorage {
            storage token;
        };

        struct ZeroRTTTokenManager {
           private:
            std::shared_ptr<void> obj;
            Token (*find_token)(void*);

           public:
        };
    }  // namespace fnet::quic::token
}  // namespace utils

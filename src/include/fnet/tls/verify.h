/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace utils {
    namespace fnet::tls {
        struct TLS;
        struct X509Store {
           private:
            const void* store;
            friend struct TLS;

            constexpr X509Store(const void* v)
                : store(v) {
            }

           public:
            void add_chain();
        };
    }  // namespace fnet::tls
}  // namespace utils

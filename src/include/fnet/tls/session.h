/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace utils {
    namespace fnet::tls {
        struct TLSConfig;
        struct TLS;
        struct Session {
           private:
            void* sess = nullptr;
            friend struct TLSConfig;
            friend struct TLS;
            constexpr Session(void* p)
                : sess(p) {
            }

           public:
            constexpr Session() = default;

            explicit constexpr operator bool() const {
                return sess!=nullptr;
            }
        };
    }  // namespace fnet::tls
}  // namespace utils

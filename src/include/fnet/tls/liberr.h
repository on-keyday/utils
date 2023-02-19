/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../dll/dllh.h"
#include "../../helper/pushbacker.h"
#include "../error.h"

namespace utils {
    namespace fnet::tls {

        struct fnet_class_export LibSubError {
            std::uint32_t code = 0;
            error::Error alt_err;
            void error(helper::IPushBacker<> pb) const;
            error::ErrorCategory category() const {
                return error::ErrorCategory::sslerr;
            }
            error::Error unwrap() const {
                return alt_err;
            }
        };

        // LibError is wrapper of OpenSSL/BoringSSL error
        struct fnet_class_export LibError {
            const char* method = nullptr;
            std::uint32_t code = 0;
            std::uint32_t ssl_code = 0;
            error::Error alt_err;

            void error(helper::IPushBacker<> pb) const;
            constexpr std::uint64_t errnum() const {
                return code;
            }

            error::Error unwrap() const {
                return alt_err;
            }

            error::ErrorCategory category() const {
                return error::ErrorCategory::sslerr;
            }
        };

        // for internal use
        // if ssl is not nullptr, LibError::ssl_code also set
        error::Error libError(const char* method, const char* msg, void* ssl = nullptr, int res = 1);
    }  // namespace fnet::tls
}  // namespace utils

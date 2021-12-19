/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp connection
#pragma once

#include "../dns/dns.h"

namespace utils {
    namespace net {
        namespace internal {
            struct TCPImpl;
        };

        struct TCPResult {
            constexpr TCPResult() {}

           private:
            internal::TCPImpl* impl = nullptr;
        };

        TCPResult open(wrap::shared_ptr<Address>&& addr);

        TCPResult accept(wrap::shared_ptr<Address>&& addr);
    }  // namespace net
}  // namespace utils

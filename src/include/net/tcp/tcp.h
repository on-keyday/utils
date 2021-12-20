/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp connection
#pragma once

#include "../dns/dns.h"
#include "../core/iodef.h"

namespace utils {
    namespace net {
        namespace internal {
            struct TCPImpl;
        };

        struct TCPConn {
            friend struct TCPResult;
            constexpr TCPConn() {}

            State write(const char* ptr, size_t size);
            State read(char* ptr, size_t size, size_t* red);
            void close();

           private:
            internal::TCPImpl* impl = nullptr;
        };

        struct TCPResult {
            friend TCPResult open(wrap::shared_ptr<Address>&& addr);
            constexpr TCPResult() {}

            TCPResult(TCPResult&&);

            TCPResult& operator=(TCPResult&&);

            wrap::shared_ptr<TCPConn> connect();

            bool failed();

            ~TCPResult();

           private:
            internal::TCPImpl* impl = nullptr;
        };

        struct TCPServer {
        };

        TCPResult open(wrap::shared_ptr<Address>&& addr);

        TCPServer setup(wrap::shared_ptr<Address>&& addr);
    }  // namespace net
}  // namespace utils

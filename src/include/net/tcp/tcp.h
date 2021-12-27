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
            friend struct TCPServer;
            constexpr TCPConn() {}

            State write(const char* ptr, size_t size);
            State read(char* ptr, size_t size, size_t* red);
            State close(bool);

            size_t get_raw();

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
            constexpr TCPServer() {}
            friend TCPServer setup(wrap::shared_ptr<Address>&& addr, int ipver);

            wrap::shared_ptr<TCPConn> accept();

            bool wait(long sec);

            TCPServer(TCPServer&&);
            TCPServer& operator=(TCPServer&&);

            TCPServer(const TCPServer&) = delete;
            TCPServer& operator=(const TCPServer&) = delete;

            ~TCPServer();

           private:
            internal::TCPImpl* impl = nullptr;
        };

        TCPResult open(wrap::shared_ptr<Address>&& addr);

        TCPServer setup(wrap::shared_ptr<Address>&& addr, int ipver = 0);
    }  // namespace net
}  // namespace utils

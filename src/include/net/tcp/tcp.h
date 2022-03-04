/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp connection
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "../dns/dns.h"
#include "../core/iodef.h"

#include "../../../include/async/worker.h"

namespace utils {
    namespace net {
        namespace internal {
            struct TCPImpl;
        };

        struct DLL TCPConn {
            friend struct DLL TCPResult;
            friend struct DLL TCPServer;
            constexpr TCPConn() {}

            State write(const char* ptr, size_t size, size_t* written);
            State read(char* ptr, size_t size, size_t* red);
            State close(bool);

            async::Future<ReadInfo> read(char* ptr, size_t size);
            async::Future<WriteInfo> write(const char* ptr, size_t size);

            size_t get_raw() const;

           private:
            internal::TCPImpl* impl = nullptr;
        };

        struct DLL TCPResult {
            friend DLL TCPResult STDCALL open(wrap::shared_ptr<Address>&& addr);
            constexpr TCPResult() {}

            TCPResult(TCPResult&&);

            TCPResult& operator=(TCPResult&&);

            wrap::shared_ptr<TCPConn> connect();

            bool failed();

            ~TCPResult();

           private:
            internal::TCPImpl* impl = nullptr;
        };

        struct DLL TCPServer {
            constexpr TCPServer() {}
            friend DLL TCPServer STDCALL setup(wrap::shared_ptr<Address>&& addr, int ipver);

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

        DLL TCPResult STDCALL open(wrap::shared_ptr<Address>&& addr);

        DLL TCPServer STDCALL setup(wrap::shared_ptr<Address>&& addr, int ipver = 0);
    }  // namespace net
}  // namespace utils

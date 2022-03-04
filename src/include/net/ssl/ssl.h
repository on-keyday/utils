/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl - ssl wrapper
// depends on OpenSSL
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "../core/iodef.h"

#include "../../wrap/lite/smart_ptr.h"

#include "../generate/iocloser.h"

namespace utils {
    namespace net {
        namespace internal {
            struct SSLImpl;
            struct SSLSyncImpl;
            struct SSLAsyncImpl;
            struct SSLSet;
        }  // namespace internal

        struct DLL SSLConn {
            friend struct DLL SSLResult;

            constexpr SSLConn() {}

            State write(const char* ptr, size_t size, size_t* written);
            State read(char* ptr, size_t size, size_t* red);
            State close(bool force = false);

            ~SSLConn();

           private:
            internal::SSLSyncImpl* impl = nullptr;
        };

        struct DLL SSLAsyncConn {
            friend internal::SSLSet;
            async::Future<ReadInfo> read(char* ptr, size_t size);
            async::Future<WriteInfo> write(const char* ptr, size_t size);
            State close(bool force);

           private:
            internal::SSLAsyncImpl* impl = nullptr;
        };

        struct SSLServer;

        struct DLL SSLResult {
            friend DLL SSLResult STDCALL open(IO&& io, const char* cert, const char* alpn, const char* host,
                                              const char* selfcert, const char* selfprivate);
            friend SSLServer;
            constexpr SSLResult() {}

            SSLResult(SSLResult&&);

            SSLResult& operator=(SSLResult&&);

            SSLResult(const SSLResult&) = delete;

            SSLResult& operator=(const SSLResult&) = delete;

            wrap::shared_ptr<SSLConn> connect();

            bool failed();

            ~SSLResult();

           private:
            internal::SSLSyncImpl* impl = nullptr;
        };

        struct DLL SSLServer {
            friend DLL SSLServer STDCALL setup(const char* selfcert, const char* selfprivate, const char* cert);
            SSLResult accept(IO&& io);

            constexpr SSLServer() {}

            SSLServer(SSLServer&&);

            SSLServer& operator=(SSLServer&&);

           private:
            internal::SSLSyncImpl* impl = nullptr;
        };

        DLL SSLResult STDCALL open(IO&& io, const char* cert, const char* alpn = nullptr, const char* host = nullptr,
                                   const char* selfcert = nullptr, const char* selfprivate = nullptr);
        DLL async::Future<wrap::shared_ptr<SSLAsyncConn>> STDCALL open_async(AsyncIOClose&& io,
                                                                             const char* cert, const char* alpn = nullptr, const char* host = nullptr,
                                                                             const char* selfcert = nullptr, const char* selfprivate = nullptr);
        DLL SSLServer STDCALL setup(const char* selfcert, const char* selfprivate, const char* cert = nullptr);
    }  // namespace net
}  // namespace utils

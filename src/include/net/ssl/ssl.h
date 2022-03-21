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
#include "../../wrap/lite/enum.h"

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

            ReadInfo read(async::Context& ctx, char* ptr, size_t size);
            WriteInfo write(async::Context& ctx, const char* ptr, size_t size);
            State close(bool force);

            void* get_raw_ssl();
            void* get_raw_sslctx();

            bool verify();

            const char* alpn_selected(int* len);

            ~SSLAsyncConn();

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

        enum class SSLAsyncError {
            none,
            set_up_error,
            cert_register_error,
            host_register_error,
            alpn_register_error,
            connect_error,
        };

        BEGIN_ENUM_STRING_MSG(SSLAsyncError, error_msg)
        ENUM_STRING_MSG(SSLAsyncError::set_up_error, "create ssl context suite failed")
        ENUM_STRING_MSG(SSLAsyncError::cert_register_error, "failed to register cert files")
        ENUM_STRING_MSG(SSLAsyncError::host_register_error, "failed to register host or alpn name")
        ENUM_STRING_MSG(SSLAsyncError::connect_error, "negotiate ssl connection failed")
        END_ENUM_STRING_MSG(nullptr)

        struct SSLAsyncResult {
            SSLAsyncError err = {};
            int errcode = 0;
            int transporterr = 0;
            wrap::shared_ptr<SSLAsyncConn> conn;
        };

        DLL SSLResult STDCALL open(IO&& io, const char* cert, const char* alpn = nullptr, const char* host = nullptr,
                                   const char* selfcert = nullptr, const char* selfprivate = nullptr);
        DLL async::Future<SSLAsyncResult> STDCALL open_async(AsyncIOClose&& io,
                                                             const char* cert, const char* alpn = nullptr, const char* host = nullptr,
                                                             const char* selfcert = nullptr, const char* selfprivate = nullptr);
        DLL SSLServer STDCALL setup(const char* selfcert, const char* selfprivate, const char* cert = nullptr);
    }  // namespace net
}  // namespace utils

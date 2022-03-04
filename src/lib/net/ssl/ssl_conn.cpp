/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "ssl_internal.h"

namespace utils {
    namespace net {
#ifndef USE_OPENSSL
        std::shared_ptr<SSLConn> SSLResult::connect() {
            return nullptr;
        }

        SSLResult open(IO&& io) {
            IO _ = std::move(io);
            return {};
        }

        State SSLConn::write(const char* ptr, size_t size) {
            return State::undefined;
        }
        State SSLConn::read(char* ptr, size_t size, size_t* red) {
            return State::undefined;
        }

        State SSLConn::close(bool force) {
            return State::undefined;
        }

        bool SSLResult::failed() {
            return false;
        }

        SSLConn::~SSLConn() {}

        SSLResult SSLServer::accept(IO&& io) {
            return SSLResult();
        }

        SSLServer::SSLServer(SSLServer&&) {
        }

        SSLServer& SSLServer::operator=(SSLServer&&) {
            return *this;
        }
#else

        State SSLConn::write(const char* ptr, size_t size, size_t* written) {
            if (!impl) {
                return State::failed;
            }
            if (impl->io_mode == internal::IOMode::idle) {
                impl->io_mode = internal::IOMode::write;
            }
            else if (impl->io_mode != internal::IOMode::write) {
                return State::invalid_argument;
            }
        BEGIN:
            if (impl->iostate == State::complete) {
                size_t w = 0;
                auto res = ::SSL_write_ex(impl->ssl, ptr, size, &w);
                if (!res) {
                    if (!need_io(impl->ssl)) {
                        impl->iostate = State::failed;
                        return State::failed;
                    }
                }
                else {
                    impl->io_progress += w;
                }
                impl->iostate = State::running;
            }
            if (impl->iostate == State::running) {
                impl->iostate = impl->do_IO();
                if (impl->iostate == State::complete) {
                    if (impl->io_progress == size) {
                        impl->io_progress = 0;
                        if (written) {
                            *written = size;
                        }
                        impl->io_mode = internal::IOMode::idle;
                        return State::complete;
                    }
                    goto BEGIN;
                }
            }
            return impl->iostate;
        }

        async::Future<ReadInfo> SSLAsyncConn::read(char* ptr, size_t size) {
            if (!ptr || !size) {
                return nullptr;
            }
        }
        async::Future<WriteInfo> write(const char* ptr, size_t size);

        State SSLConn::read(char* ptr, size_t size, size_t* red) {
            if (!impl) {
                return State::failed;
            }
            if (impl->io_mode == internal::IOMode::idle) {
                impl->io_mode = internal::IOMode::read;
            }
            else if (impl->io_mode != internal::IOMode::read) {
                return State::invalid_argument;
            }
            size_t repeat = 0;
        BEGIN:
            if (impl->iostate == State::complete) {
                size_t w = 0;
                auto res = ::SSL_read_ex(impl->ssl, ptr, size, &w);
                if (!res) {
                    if (!need_io(impl->ssl)) {
                        impl->iostate = State::failed;
                        return State::failed;
                    }
                }
                else {
                    impl->io_mode = internal::IOMode::idle;
                    if (red) {
                        *red = w;
                    }
                    return State::complete;
                }
                impl->iostate = State::running;
            }
            if (impl->iostate == State::running) {
                impl->iostate = impl->do_IO();
                if (impl->iostate == State::complete) {
                    repeat++;
                    if (repeat >= 50) {
                        impl->io_mode = internal::IOMode::idle;
                        return State::running;
                    }
                    goto BEGIN;
                }
            }
            return impl->iostate;
        }

        State SSLConn::close(bool force) {
            if (!impl) {
                return State::failed;
            }
            impl->io_mode = internal::IOMode::close;
            if (impl->connected) {
                size_t times = 0;
            BEGIN:
                times++;
                auto res = ::SSL_shutdown(impl->ssl);
                if (res < 0) {
                    if (need_io(impl->ssl)) {
                        auto err = impl->do_IO();
                        if (err == State::running) {
                            return State::running;
                        }
                        else if (err == State::complete) {
                            if (times <= 10) {
                                goto BEGIN;
                            }
                        }
                    }
                }
                else if (res == 0) {
                    if (times <= 10) {
                        goto BEGIN;
                    }
                }
            }
            impl->clear();
            return State::complete;
        }

        SSLConn::~SSLConn() {
            close(true);
            delete impl;
        }

        wrap::shared_ptr<SSLConn> SSLResult::connect() {
            if (!impl) {
                return nullptr;
            }
            if (failed()) {
                return nullptr;
            }
            if (connecting(impl) == State::complete) {
                auto conn = wrap::make_shared<SSLConn>();
                conn->impl = impl;
                impl = nullptr;
                return conn;
            }
            return nullptr;
        }

        bool SSLResult::failed() {
            return !impl ||
                   impl->iostate != State::complete &&
                       impl->iostate != State::running;
        }

        SSLResult::SSLResult(SSLResult&& in) {
            impl = in.impl;
            in.impl = nullptr;
        }

        SSLResult& SSLResult::operator=(SSLResult&& in) {
            delete impl;
            impl = in.impl;
            in.impl = nullptr;
            return *this;
        }

        SSLResult::~SSLResult() {
            delete impl;
        }

        SSLResult open(IO&& io, const char* cert, const char* alpn, const char* host,
                       const char* selfcert, const char* selfprivate) {
            if (!io || !cert) {
                return SSLResult();
            }
            SSLResult result;
            result.impl = new internal::SSLSyncImpl();
            if (!common_setup(result.impl, std::move(io), cert, alpn, host, selfcert, selfprivate)) {
                return SSLResult();
            }
            auto state = connecting(result.impl);
            if (state != State::complete && state != State::running) {
                return SSLResult();
            }
            return result;
        }

        SSLServer setup(const char* selfcert, const char* selfprivate, const char* cert) {
            SSLServer server;
            server.impl = new internal::SSLSyncImpl();
            server.impl->is_server = true;
            if (!common_setup(server.impl, nullptr, cert, nullptr, nullptr, selfcert, selfprivate)) {
                return SSLServer();
            }
            return server;
        }

        SSLServer::SSLServer(SSLServer&& in) {
            impl = in.impl;
            in.impl = nullptr;
        }

        SSLServer& SSLServer::operator=(SSLServer&& in) {
            delete impl;
            impl = in.impl;
            in.impl = nullptr;
            return *this;
        }

        SSLResult SSLServer::accept(IO&& io) {
            SSLResult result;
            result.impl = new internal::SSLSyncImpl();
            if (!setup_ssl(impl)) {
                return SSLResult();
            }
            result.impl->bio = impl->bio;
            impl->bio = nullptr;
            result.impl->ssl = impl->ssl;
            impl->ssl = nullptr;
            result.impl->is_server = true;
            auto state = connecting(result.impl);
            if (state != State::complete && state != State::running) {
                return SSLResult();
            }
            return result;
        }
#endif
    }  // namespace net
}  // namespace utils

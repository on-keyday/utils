/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <platform/windows/dllexport_source.h>
#include <deprecated/net/http2/conn.h>
#include <deprecated/net/async/pool.h>
#include <endian/endian.h>
#include <helper/strutil.h>
#include "frame_reader.h"
#include <async/async_macro.h>

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {

                struct Http2Impl {
                    AsyncIOClose io;
                    FrameReader<> reader;
                    H2Error err = H2Error::none;
                    int errcode = 0;
                };
            }  // namespace internal

            constexpr auto connection_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

            async::Future<OpenResult> STDCALL open_async(AsyncIOClose&& io) {
                return net::start([io = std::move(io)](async::Context& ctx) mutable {
                    return open_async(ctx, std::move(io));
                });
            }

            OpenResult STDCALL open_async(async::Context& ctx, AsyncIOClose&& io) {
                auto impl = wrap::make_shared<internal::Http2Impl>();
                impl->io = std::move(io);
                auto ptr = impl->io.write(connection_preface, 24);
                auto w = AWAIT(ptr);
                if (w.err) {
                    return OpenResult{.errcode = w.err};
                }
                auto conn = wrap::make_shared<Conn>();
                conn->impl = std::move(impl);
                return OpenResult{.conn = std::move(conn)};
            }

            void Conn::set_error(H2Error err) {
                if (impl) {
                    impl->err = err;
                }
            }

            H2Error Conn::get_error() {
                if (!impl) {
                    return H2Error::unknown;
                }
                return impl->err;
            }

            int Conn::get_errorcode() {
                if (!impl) {
                    return -1;
                }
                return impl->errcode;
            }

            AsyncIOClose Conn::get_io() {
                if (!impl) {
                    return nullptr;
                }
                return std::move(impl->io);
            }

            WriteInfo Conn::write_serial(async::Context& ctx, const wrap::string& buf) {
                return impl->io.write(ctx, buf.c_str(), buf.size());
            }

            bool Conn::serialize_frame(IBuffer& buf, const Frame& frame) {
                internal::FrameWriter<IBuffer&> w{buf};
                H2Error err = encode(&frame, w);
                if (err != H2Error::none) {
                    impl->err = err;
                    return false;
                }
                return true;
            }

            wrap::shared_ptr<Frame> Conn::read(async::Context& ctx) {
                wrap::shared_ptr<Frame> frame;
                assert(impl->reader.pos == 0);
                auto err = decode(impl->reader, frame);
                if (err == H2Error::none) {
                    impl->reader.tidy();
                    return std::move(frame);
                }
                while (true) {
                    char tmp[1024];
                    auto r = impl->io.read(tmp, 1024);
                    r.wait_until(ctx);
                    auto got = r.get();
                    if (got.err) {
                        impl->errcode = got.err;
                        impl->err = H2Error::transport;
                        return nullptr;
                    }
                    impl->reader.ref.append(tmp, got.read);
                    auto starts = [&](auto&& v) {
                        return helper::starts_with(impl->reader.ref, v);
                    };
                    if (starts("HTTP") || starts("GET ") || starts("POST") ||
                        starts("PUT ") || starts("PATCH") || starts("HEAD") ||
                        starts("TRACE") || starts("CONNECT")) {
                        impl->err = H2Error::http_1_1_required;
                        return nullptr;
                    }
                    assert(impl->reader.pos == 0);
                    wrap::shared_ptr<Frame> frame;
                    auto err = decode(impl->reader, frame);
                    if (err == H2Error::none) {
                        impl->reader.tidy();
                        return std::move(frame);
                    }
                    if (!any(err & H2Error::user_defined_bit)) {
                        impl->err = err;
                        return nullptr;
                    }
                    impl->reader.pos = 0;
                }
            }

            async::Future<wrap::shared_ptr<Frame>> Conn::read() {
                return get_pool().start<wrap::shared_ptr<Frame>>([this](async::Context& ctx) {
                    return this->read(ctx);
                });
            }

            bool Conn::write(async::Context& ctx, const Frame& frame) {
                internal::FrameWriter w;
                H2Error err = encode(&frame, w);
                if (err != H2Error::none) {
                    impl->err = err;
                    return false;
                }
                auto got = impl->io.write(ctx, w.str.c_str(), w.str.size());
                if (got.err) {
                    impl->errcode = got.err;
                    impl->err = H2Error::transport;
                    return false;
                }
                return true;
            }

            async::Future<bool> Conn::write(const Frame& frame) {
                internal::FrameWriter w;
                H2Error err = encode(&frame, w);
                if (err != H2Error::none) {
                    impl->err = err;
                    return false;
                }
                return get_pool().start<bool>([str = std::move(w.str), impl = this->impl](async::Context& ctx) {
                    auto w = impl->io.write(str.c_str(), str.size());
                    w.wait_until(ctx);
                    auto got = w.get();
                    if (got.err) {
                        impl->errcode = got.err;
                        impl->err = H2Error::transport;
                        return;
                    }
                    ctx.set_value(true);
                });
            }

            State Conn::close(bool force) {
                return impl->io.close(force);
            }

            Conn::~Conn() {
                close(true);
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils

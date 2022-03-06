/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/http2/conn.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/endian/endian.h"

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {

                struct FrameWriter {
                    wrap::string str;
                    template <class T>
                    void write(T& val, size_t size = sizeof(T)) {
                        auto nt = endian::to_network(&val);
                        char* ptr = reinterpret_cast<char*>(&nt);
                        for (auto i = sizeof(T) - size; i < sizeof(T); i++) {
                            str.push_back(ptr[i]);
                        }
                    }

                    void write(const wrap::string& val, size_t sz) {
                        str.append(val, 0, sz);
                    }
                    void write(const Dummy&) {}
                };
                struct FrameReader {
                    wrap::string ref;
                    size_t pos = 0;

                    void tidy() {
                        ref.erase(0, pos);
                        pos = 0;
                    }

                    bool read(Dummy&) {
                        return true;
                    }

                    template <class T>
                    bool read(T& t, size_t size = sizeof(T)) {
                        if (size > sizeof(T)) {
                            return false;
                        }
                        T cvt;
                        char* ptr = reinterpret_cast<char*>(std::addressof(cvt));
                        for (size_t i = sizeof(T) - size; i < sizeof(T); i++) {
                            if (pos >= ref.size()) {
                                return false;
                            }
                            ptr[i] = ref[pos];
                            pos++;
                        }
                        t = endian::from_network(&cvt);
                        return true;
                    }

                    bool read(wrap::string& str, size_t size) {
                        if (ref.size() - pos < size) {
                            return false;
                        }
                        auto first = ref.begin() + pos;
                        auto second = ref.begin() + pos + size;
                        str.append(first, second);
                        pos += size;
                        return true;
                    }
                };

                struct Http2Impl {
                    AsyncIOClose io;
                    FrameReader reader;
                    H2Error err;
                    int errcode;
                };
            }  // namespace internal

            constexpr auto connection_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

            async::Future<wrap::shared_ptr<Conn>> STDCALL open_async(AsyncIOClose&& io) {
                auto impl = wrap::make_shared<internal::Http2Impl>();
                impl->io = std::move(io);
                return get_pool().start<wrap::shared_ptr<Conn>>([impl = std::move(impl)](async::Context& ctx) {
                    auto ptr = impl->io.write(connection_preface, 24);
                    auto w = ptr.get();
                    if (w.err) {
                        return;
                    }
                    auto conn = wrap::make_shared<Conn>();
                    conn->impl = std::move(impl);
                    ctx.set_value(std::move(conn));
                });
            }

            async::Future<wrap::shared_ptr<Frame>> Conn::read() {
                wrap::shared_ptr<Frame> frame;
                assert(impl->reader.pos == 0);
                auto err = decode(impl->reader, frame);
                if (err == H2Error::none) {
                    impl->reader.tidy();
                    return {std::move(frame)};
                }
                if (!any(err & H2Error::user_defined_bit)) {
                    impl->err = err;
                    return nullptr;
                }
                impl->reader.pos = 0;
                return get_pool().start<wrap::shared_ptr<Frame>>([impl = this->impl](async::Context& ctx) {
                    while (true) {
                        char tmp[1024];
                        auto r = impl->io.read(tmp, 1024);
                        r.wait_or_suspend(ctx);
                        auto got = r.get();
                        if (got.err) {
                            impl->errcode = got.err;
                            impl->err = H2Error::transport;
                            return;
                        }
                        impl->reader.ref.append(tmp, got.read);
                        assert(impl->reader.pos == 0);
                        wrap::shared_ptr<Frame> frame;
                        auto err = decode(impl->reader, frame);
                        if (err == H2Error::none) {
                            impl->reader.tidy();
                            return ctx.set_value(std::move(frame));
                        }
                        if (!any(err & H2Error::user_defined_bit)) {
                            impl->err = err;
                            return;
                        }
                        impl->reader.pos = 0;
                    }
                });
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
                    w.wait_or_suspend(ctx);
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

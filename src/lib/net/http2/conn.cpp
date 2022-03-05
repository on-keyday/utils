/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/http2/conn.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/endian/endian.h"

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {
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
                        for (size_t i = size - sizeof(T); i < size; i++) {
                            if (pos >= buf.size()) {
                                return false;
                            }
                            ptr[i] = ref[pos];
                            pos++;
                        }
                        t = endian::from_network_us<T>(&cvt);
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
                    }
                };

                struct Http2Impl {
                    AsyncIOClose io;
                    FrameReader reader;
                    H2Error err;
                    int errcode;
                };
            }  // namespace internal

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
        }  // namespace http2
    }      // namespace net
}  // namespace utils

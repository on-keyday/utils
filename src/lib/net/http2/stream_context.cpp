/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/http2/stream.h"
#include "../../../include/async/async_macro.h"
#include "../../../include/net/async/pool.h"

namespace utils {
    namespace net {
        namespace http2 {
            async::Future<wrap::shared_ptr<Context>> STDCALL negotiate(wrap::shared_ptr<Conn>&& conn, SettingsFrame& frame) {
                auto ctx = wrap::make_shared<Context>();
                ctx->io = std::move(conn);
                auto f = ctx->write(frame);
                return start(
                    [](async::Context& ctx, wrap::shared_ptr<Context> h2ctx, async::Future<H2Error> f) {
                        auto err = AWAIT(f);
                        if (err != H2Error::none) {
                            return h2ctx;
                        }
                        bool recvack = false;
                        bool sendack = false;
                        while (true) {
                            auto ptr = AWAIT(h2ctx->read());
                            if (!ptr) {
                                return h2ctx;
                            }
                            if (ptr->type == FrameType::goaway) {
                                return h2ctx;
                            }
                            else if (ptr->type == FrameType::ping) {
                                if (!(ptr->flag & Flag::ack)) {
                                    ptr->flag |= Flag::ack;
                                    AWAIT(h2ctx->write(*ptr));
                                }
                            }
                            else if (ptr->type == FrameType::settings) {
                                if (ptr->flag & Flag::ack) {
                                    recvack = true;
                                }
                                else {
                                    SettingsFrame sf{0};
                                    sf.type = FrameType::settings;
                                    sf.flag = Flag::ack;
                                    AWAIT(h2ctx->write(sf));
                                    sendack = true;
                                }
                            }
                            if (recvack && sendack) {
                                break;
                            }
                        }
                        return h2ctx;
                    },
                    ctx, std::move(f));
            }

            async::Future<H2Error> Context::write(const Frame& frame) {
                if (auto e = state.update_send(frame); e != H2Error::none) {
                    return e;
                }
                auto fu = io->write(frame);
                auto fn = [&io = this->io](async::Context& ctx, async::Future<bool> f) {
                    if (!AWAIT(f)) {
                        return io->get_error();
                    }
                    return H2Error::none;
                };
                return start(fn, std::move(fu));
            }

            async::Future<wrap::shared_ptr<Frame>> Context::read() {
                auto fn = [&io = this->io, &state = this->state](async::Context& ctx)
                    -> wrap::shared_ptr<Frame> {
                    auto r = AWAIT(io->read());
                    if (!r) {
                        return nullptr;
                    }
                    if (auto err = state.update_recv(*r); err != H2Error::none) {
                        io->set_error(err);
                        return nullptr;
                    }
                    return r;
                };
                return start(fn);
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils

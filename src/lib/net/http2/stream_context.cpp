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
            async::Future<NegotiateResult> STDCALL negotiate(wrap::shared_ptr<Conn>&& conn, SettingsFrame& frame) {
                auto ctx = wrap::make_shared<Context>();
                ctx->io = std::move(conn);
                auto f = ctx->write(frame);
                return start(
                    [](async::Context& ctx, wrap::shared_ptr<Context> h2ctx, async::Future<UpdateResult> f)
                        -> NegotiateResult {
                        auto err = AWAIT(f);
                        if (err.err != H2Error::none) {
                            return {
                                .err = std::move(err),
                                .ctx = std::move(h2ctx),
                            };
                        }
                        bool recvack = false;
                        bool sendack = false;
                        while (true) {
                            auto res = AWAIT(h2ctx->read());
                            if (res.err.err != H2Error::none) {
                                return {.err = res.err};
                            }
                            auto ptr = res.frame;
                            if (ptr->type == FrameType::goaway) {
                                return {.ctx = std::move(h2ctx)};
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
                        return {.ctx = std::move(h2ctx)};
                    },
                    ctx, std::move(f));
            }

            async::Future<UpdateResult> Context::write(const Frame& frame) {
                if (auto e = state.update_send(frame); e.err != H2Error::none) {
                    return e;
                }
                auto fu = io->write(frame);
                auto fn = [&io = this->io](async::Context& ctx, async::Future<bool> f) {
                    if (!AWAIT(f)) {
                        return UpdateResult{
                            .err = H2Error::transport,
                            .detail = StreamError::writing_frame,
                        };
                    }
                    return UpdateResult{};
                };
                return start(fn, std::move(fu));
            }

            async::Future<ReadResult> Context::read() {
                auto fn = [&io = this->io, &state = this->state](async::Context& ctx)
                    -> ReadResult {
                    auto r = AWAIT(io->read());
                    if (!r) {
                        return {.err = UpdateResult{
                                    .err = H2Error::transport,
                                    .detail = StreamError::reading_frame,
                                }};
                    }
                    if (auto err = state.update_recv(*r); err.err != H2Error::none) {
                        return {.err = err, .frame = r};
                    }
                    return {.frame = r};
                };
                return start(fn);
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils

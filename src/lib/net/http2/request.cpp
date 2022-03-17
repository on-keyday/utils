/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/http2/request.h"
#include "../http/header_impl.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/async/async_macro.h"

namespace utils {
    namespace net {
        namespace http2 {
            async::Future<UpdateResult> send_header(wrap::shared_ptr<Context>& h2ctx, http::Header&& h, bool end_stream) {
                auto fn = [](async::Context& ctx, wrap::shared_ptr<Context> h2ctx, http::Header h, bool end_stream) -> UpdateResult {
                    HeaderFrame frame{0};
                    wrap::string remain;
                    if (!h2ctx->state.make_header(std::move(h), frame, remain)) {
                        return UpdateResult{
                            .err = H2Error::internal,
                            .detail = StreamError::hpack_failed,
                            .id = frame.id,
                        };
                    }
                    if (end_stream) {
                        frame.flag |= Flag::end_stream;
                    }
                    auto r = AWAIT(h2ctx->write(frame));
                    if (r.err != H2Error::none) {
                        return std::move(r);
                    }
                    while (remain.size()) {
                        Continuation cont;
                        if (!h2ctx->state.make_continuous(frame.id, remain, cont)) {
                            return UpdateResult{
                                .err = H2Error::internal,
                                .detail = StreamError::continuation_not_followed,
                                .id = frame.id,
                            };
                        }
                        r = AWAIT(h2ctx->write(cont));
                        if (r.err != H2Error::none) {
                            return std::move(r);
                        }
                    }
                    return {.id = frame.id};
                };
                return start(fn, h2ctx, std::move(h), end_stream);
            }

            async::Future<H2Response> STDCALL request(wrap::shared_ptr<Context> ctx, http::Header&& h, const wrap::string& data) {
                if (!ctx || !h) {
                    return nullptr;
                }
                auto fn = [](async::Context& ctx, wrap::shared_ptr<Context> h2ctx, wrap::string data, http::Header h)
                    -> H2Response {
                    auto write_goaway = [&](H2Error err) {
                        if (err != H2Error::transport) {
                            GoAwayFrame goaway;
                            goaway.type = FrameType::goaway;
                            goaway.processed_id = 0;
                            goaway.code = (std::uint32_t)err;
                            AWAIT(h2ctx->write(goaway));
                        }
                    };
                    auto err = AWAIT(send_header(h2ctx, std::move(h), !data.size()));
                    if (err.err != H2Error::none) {
                        return {.err = std::move(err)};
                    }
                    auto id = err.id;
                    auto recv_frame = [&] {
                        auto res = AWAIT(h2ctx->read());
                        if (res.err.err != H2Error::none) {
                            write_goaway(res.err.err);
                            return false;
                        }
                        auto rframe = res.frame;
                        if (rframe->type == FrameType::ping) {
                            if (!(rframe->flag & Flag::ack)) {
                                rframe->flag |= Flag::ack;
                                AWAIT(h2ctx->write(*rframe));
                            }
                        }
                        else if (rframe->type == FrameType::data) {
                            WindowUpdateFrame update{0};
                            update.type = FrameType::window_update;
                            update.increment = rframe->len;
                            AWAIT(h2ctx->write(update));
                            update.id = rframe->id;
                            AWAIT(h2ctx->write(update));
                        }
                        return true;
                    };
                    bool block = false;
                    while (data.size()) {
                        if (block) {
                            if (!recv_frame()) {
                                return {};
                            }
                        }
                        DataFrame dframe;
                        if (!h2ctx->state.make_data(id, data, dframe, block)) {
                            if (!block) {
                                write_goaway(H2Error::internal);
                                return {};
                            }
                            continue;
                        }
                        if (auto err = AWAIT(h2ctx->write(dframe)); err.err != H2Error::none) {
                            write_goaway(err.err);
                            return {};
                        }
                    }
                    auto stream = h2ctx->state.stream(id);
                    while (stream->status() != Status::closed) {
                        if (!recv_frame()) {
                            return {};
                        }
                    }
                    return {.err = UpdateResult{.id = id}, .resp = stream->response()};
                };
                return start(fn, std::move(ctx), std::move(data), std::move(h));
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils

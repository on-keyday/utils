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
            H2Result default_handle_recv(async::Context& ctx) {
            }

            async::Future<http::HttpAsyncResponse> STDCALL request(wrap::shared_ptr<Context> ctx, http::Header&& h, const wrap::string& data) {
                if (!ctx || !h) {
                    return nullptr;
                }
                HeaderFrame frame;
                wrap::string remain;
                if (!ctx->state.make_header(std::move(h), frame, remain)) {
                    return nullptr;
                }
                auto fn = [](async::Context& ctx, wrap::shared_ptr<Context> h2ctx, wrap::string remain, HeaderFrame frame, wrap::string data)
                    -> http::HttpAsyncResponse {
                    auto write_goaway = [&](H2Error err) {
                        if (err != H2Error::transport) {
                            GoAwayFrame goaway;
                            goaway.type = FrameType::goaway;
                            goaway.processed_id = 0;
                            goaway.code = (std::uint32_t)err;
                            AWAIT(h2ctx->write(goaway));
                        }
                    };
                    if (!data.size()) {
                        frame.flag |= Flag::end_stream;
                    }
                    if (auto err = AWAIT(h2ctx->write(frame)); err.err != H2Error::none) {
                        write_goaway(err.err);
                        return {};
                    }
                    auto recv_frame = [&] {
                        auto res = AWAIT(h2ctx->read());
                        if (rframe.err.err != H2Error::none) {
                            write_goaway(rframe.err.err);
                            return false;
                        }
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
                        }
                        return true;
                    };
                    bool block = false;
                    while (true) {
                        if (remain.size()) {
                            Continuation cont;
                            if (!h2ctx->state.make_continuous(frame.id, remain, cont)) {
                                write_goaway(H2Error::internal);
                                return {};
                            }
                            if (auto err = AWAIT(h2ctx->write(cont)); err != H2Error::none) {
                                write_goaway(err);
                                return {};
                            }
                        }
                        else if (data.size()) {
                            if (block) {
                                if (!recv_frame()) {
                                    return {};
                                }
                            }
                            DataFrame dframe;
                            if (!h2ctx->state.make_data(frame.id, data, dframe, block)) {
                                if (!block) {
                                    write_goaway(H2Error::internal);
                                    return {};
                                }
                                continue;
                            }
                            if (auto err = AWAIT(h2ctx->write(dframe)); err != H2Error::none) {
                                write_goaway(err);
                                return {};
                            }
                        }
                        else {
                            break;
                        }
                    }
                    auto stream = h2ctx->state.stream(frame.id);
                    while (stream->status() != Status::closed) {
                        if (!recv_frame()) {
                            return {};
                        }
                    }
                    return stream->response();
                };
                return start(fn, std::move(ctx), std::move(remain), std::move(frame), std::move(data));
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils

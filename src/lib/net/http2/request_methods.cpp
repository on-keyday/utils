/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/http2/request_methods.h"
#include "../../../include/async/async_macro.h"

namespace utils {
    namespace net {
        namespace http2 {
            ReadResult STDCALL default_handle_ping_and_data(async::Context& ctx, const wrap::shared_ptr<Context>& h2ctx) {
                auto res = AWAIT(h2ctx->read());
                if (res.err.err != H2Error::none || !res.frame) {
                    return std::move(res);
                }
                auto err = handle_ping(ctx, h2ctx, *res.frame);
                if (err.err != H2Error::none) {
                    return {.err = std::move(err), .frame = std::move(res.frame)};
                }
                if (res.frame->type == FrameType::data && res.frame->len != 0) {
                    auto id = res.frame->id;
                    auto len = res.frame->len;
                    err = update_window_async(ctx, h2ctx, id, len);
                    if (err.err != H2Error::none) {
                        return {.err = std::move(err), .frame = std::move(res.frame)};
                    }
                    err = update_window_async(ctx, h2ctx, 0, len);
                    if (err.err != H2Error::none) {
                        return {.err = std::move(err), .frame = std::move(res.frame)};
                    }
                }
                return std::move(res);
            }

            UpdateResult STDCALL handle_ping(async::Context& ctx, const wrap::shared_ptr<Context>& h2ctx, Frame& frame) {
                if (frame.type != FrameType::ping) {
                    return {};
                }
                if (frame.flag & Flag::ack) {
                    return {};
                }
                frame.flag |= Flag::ack;
                auto res = AWAIT(h2ctx->write(frame));
                frame.flag &= ~Flag::ack;
                return res;
            }

            UpdateResult STDCALL update_window_async(async::Context& ctx, const wrap::shared_ptr<Context>& h2ctx, std::int32_t id, std::uint32_t incr) {
                if (id < 0 || incr == 0) {
                    return {
                        .err = H2Error::protocol,
                        .detail = StreamError::require_id_not_0,
                        .id = id,
                        .frame = FrameType::window_update,
                    };
                }
                WindowUpdateFrame wframe{0};
                wframe.id = id;
                wframe.type = FrameType::window_update;
                wframe.increment = incr;
                wframe.len = 4;
                return AWAIT(h2ctx->write(wframe));
            }

            ReadResult STDCALL wait_data_async(async::Context& ctx, const wrap::shared_ptr<Context>& h2ctx, std::int32_t id, wrap::string* ptr, bool end_stream) {
                if (!ptr || !ptr->size() || id <= 0) {
                    return ReadResult{};
                }
                DataFrame dframe{0};
                dframe.type = FrameType::data;
                if (end_stream) {
                    dframe.flag |= Flag::end_stream;
                }
                ReadResult res;
                bool block = false, first = true;
                while (ptr->size()) {
                    auto r = h2ctx->state.make_data(id, *ptr, dframe, block);
                    if (!r) {
                        if (!block) {
                            return ReadResult{
                                .err = {
                                    .err = H2Error::internal,
                                    .detail = StreamError::internal_data_read,
                                    .id = id,
                                    .frame = FrameType::data,
                                },
                                .frame = std::move(res.frame),
                            };
                        }
                        if (!first) {
                            return std::move(res);
                        }
                        res = AWAIT(h2ctx->read());
                        if (res.err.err != H2Error::none) {
                            return std::move(res);
                        }
                        if (res.frame->id == id &&
                            res.frame->type == FrameType::window_update) {
                            first = false;
                            continue;
                        }
                        return std::move(res);
                    }
                    if (dframe.flag & Flag::end_stream && !end_stream) {
                        dframe.flag &= ~Flag::end_stream;
                    }
                    auto err = AWAIT(h2ctx->write(dframe));
                    if (err.err != H2Error::none) {
                        return {.err = std::move(err)};
                    }
                }
                return {};
            }

            UpdateResult STDCALL send_header_async(async::Context& ctx, const wrap::shared_ptr<Context>& h2ctx, http::Header h, bool end_stream) {
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
            }

        }  // namespace http2
    }      // namespace net
}  // namespace utils

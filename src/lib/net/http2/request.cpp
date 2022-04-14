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
#include "../../../include/net/http2/request_methods.h"

namespace utils {
    namespace net {
        namespace http2 {

            async::Future<UpdateResult> send_header(wrap::shared_ptr<Context>& h2ctx, http::Header&& h, bool end_stream) {
                return start(send_header_async, h2ctx, std::move(h), end_stream);
            }

            H2Response STDCALL request(async::Context& ctx, wrap::shared_ptr<Context> h2ctx, http::Header&& h, const wrap::string& data) {
                auto write_goaway = [&](H2Error err) {
                    if (err != H2Error::transport) {
                        send_goaway(ctx, h2ctx, (std::uint32_t)err);
                    }
                };
                auto err = send_header_async(ctx, h2ctx, std::move(h), !data.size());
                if (err.err != H2Error::none) {
                    return {.err = std::move(err)};
                }
                auto id = err.id;
                auto recv_frame = [&] {
                    auto res = default_handling_ping_and_data(ctx, h2ctx);
                    if (res.err.err != H2Error::none) {
                        write_goaway(res.err.err);
                    }
                    return std::move(res);
                };
                bool block = false;
                auto datacpy = data;
                while (datacpy.size()) {
                    auto res = wait_data_async(ctx, h2ctx, id, &datacpy, true);
                    if (res.err.err != H2Error::none) {
                        return {.err = std::move(res.err)};
                    }
                }
                auto stream = h2ctx->state.stream(id);
                while (stream->status() != Status::closed) {
                    if (auto res = recv_frame(); res.err.err != H2Error::none) {
                        return {.err = std::move(res.err)};
                    }
                }
                return {.err = UpdateResult{.id = id}, .resp = stream->response()};
            }

            async::Future<H2Response> STDCALL request(wrap::shared_ptr<Context> ctx, http::Header&& h, const wrap::string& data) {
                if (!ctx || !h) {
                    return nullptr;
                }
                auto fn = [](async::Context& ctx, wrap::shared_ptr<Context> h2ctx, wrap::string data, http::Header h)
                    -> H2Response {
                    return request(ctx, std::move(h2ctx), std::move(h), data);
                };
                return start(fn, std::move(ctx), std::move(data), std::move(h));
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils

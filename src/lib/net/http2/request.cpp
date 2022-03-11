/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/http2/request.h"
#include "../http/header_impl.h"
#include "../../../include/net/async/pool.h"
#include "../../../include/async/async_macro.h"

namespace utils {
    namespace net {
        namespace http2 {
            async::Future<http::HttpAsyncResponse> request(wrap::shared_ptr<Conn>&& io, Connection* conn, http::Header&& h, const wrap::string& data) {
                if (!io || !conn || !h) {
                    return nullptr;
                }
                HeaderFrame frame;
                wrap::string remain;
                if (!conn->make_header(std::move(h), frame, remain)) {
                    return nullptr;
                }
                auto fn = [](async::Context& ctx, wrap::shared_ptr<Conn>&& io, Connection* conn, wrap::string remain, HeaderFrame frame, const wrap::string& data)
                    -> http::HttpAsyncResponse {
                    if (!data.size()) {
                        frame.flag |= Flag::end_stream;
                    }
                    if (!AWAIT(io->write(frame))) {
                        if (io->get_error() != H2Error::transport) {
                            GoAwayFrame goaway;
                            goaway.type = FrameType::goaway;
                            goaway.processed_id = 0;
                            goaway.code = (std::uint32_t)io->get_error();
                            AWAIT(io->write(goaway));
                        }
                        return {};
                    }
                    while (true) {
                    }
                };
                return async::start(get_pool(), fn, std::move(io), conn, std::move(remain), std::move(frame), std::move(data));
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils

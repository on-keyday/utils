/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "stream.h"
#include "typeconfig.h"

namespace futils::fnet::http3 {
    template <class QpackConfig, class QuicConnConfig>
    void multiplexer_on_transport_parameter_read(std::shared_ptr<void>& app, std::shared_ptr<quic::context::Context<QuicConnConfig>>&& ctx) {
        using H3Conf = http3::TypeConfig<QpackConfig, typename QuicConnConfig::stream_type_config>;
        using H3Conn = http3::stream::Connection<H3Conf>;
        auto conn = std::make_shared<H3Conn>();
        ctx->set_application_context(conn);
        auto streams = ctx->get_streams();
        auto control = streams->open_uni();
        auto qpack_encoder = streams->open_uni();
        auto qpack_decoder = streams->open_uni();
        conn->ctrl.send_control = control;
        conn->send_decoder = qpack_decoder;
        conn->send_encoder = qpack_encoder;
        if (!conn->write_uni_stream_headers_and_settings([&](auto&& write_settings) {})) {
            ctx->request_close(quic::AppError{
                std::uint64_t(http3::H3Error::H3_CLOSED_CRITICAL_STREAM),
                error::Error("cannot create unidirectional stream", error::Category::app),
            });
            return;
        }
    }

    template <class QpackConfig, class QuicConnConfig>
    void multiplexer_recv_uni_stream(std::shared_ptr<void>& app,
                                     std::shared_ptr<quic::context::Context<QuicConnConfig>>&& ctx,
                                     std::shared_ptr<quic::stream::impl::RecvUniStream<typename QuicConnConfig::stream_type_config>>&& stream) {
        using H3Conf = http3::TypeConfig<QpackConfig, typename QuicConnConfig::stream_type_config>;
        using H3Conn = http3::stream::Connection<H3Conf>;
        auto h3 = std::static_pointer_cast<H3Conn>(ctx->get_application_context());
        h3->read_uni_stream(std::move(stream));
    };
}  // namespace futils::fnet::http3
/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "stream.h"

namespace futils::fnet::quic::stream::impl {
    template <class TConfig, class Lock = typename TConfig::recv_stream_lock>
    void set_on_send_callback(SendUniStream<TConfig>& r, void (*on_send)(std::shared_ptr<void>&& conn_ctx, StreamID id)) {
        using Saver = typename TConfig::stream_handler::send_buf;
        r.get_sender_ctx()->on_data_added_cb = on_data_added_cb;
    }
}  // namespace futils::fnet::quic::stream::impl

/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "recv_stream.h"
#include "stream.h"

namespace futils::fnet::quic::stream::impl {
    template <class TConfig, class Lock = typename TConfig::recv_stream_lock>
    std::shared_ptr<RecvSorter<Lock>> set_stream_reader(RecvUniStream<TConfig>& r, bool use_auto_update = false, void (*on_data_added)(std::shared_ptr<void>&& ctx, StreamID id) = nullptr) {
        auto read = std::allocate_shared<RecvSorter<Lock>>(glheap_allocator<stream::impl::RecvSorter<Lock>>{});
        read->set_data_added_cb(on_data_added);
        using Saver = typename TConfig::stream_handler::recv_buf;
        if (use_auto_update) {
            r.set_receiver(Saver(read,
                                 reader_recv_handler<Lock, TConfig>,
                                 reader_auto_updater<Lock, TConfig>));
        }
        else {
            r.set_receiver(Saver(read, reader_recv_handler<Lock, TConfig>));
        }
        return read;
    }
}  // namespace futils::fnet::quic::stream::impl

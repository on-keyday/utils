#pragma once
#include "recv_stream.h"
#include "stream.h"

namespace futils::fnet::quic::stream::impl {
    template <class TConfig, class Lock = typename TConfig::recv_stream_lock>
    std::shared_ptr<RecvSorter<Lock>> set_stream_reader(RecvUniStream<TConfig>& r, bool use_auto_update = false) {
        auto read = std::allocate_shared<RecvSorter<Lock>>(glheap_allocator<stream::impl::RecvSorter<Lock>>{});
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

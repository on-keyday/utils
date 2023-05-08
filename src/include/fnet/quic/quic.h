/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic - QUIC implemetation
// this header provides useful aliases of type that provides QUIC protocol context
// to customize configuration, see also context/context.h
// using namespace utils::fnet::quic::use to export usable alias
#pragma once
#include "context/context.h"
#include "context/default.h"
#include "stream/impl/recv_stream.h"
#include <mutex>

namespace utils {
    namespace fnet::quic::use {
        using DefaultTypeConfig = context::TypeConfig<std::mutex>;
        using DefaultStreamTypeConfig = typename DefaultTypeConfig::stream_type_config;
        using Context = context::Context<DefaultTypeConfig>;
        using Streams = stream::impl::Conn<DefaultStreamTypeConfig>;
        using RecvStream = stream::impl::RecvUniStream<DefaultStreamTypeConfig>;
        using Reader = stream::impl::RecvSorted<std::mutex>;
        using SendStream = stream::impl::SendUniStream<DefaultStreamTypeConfig>;
        using BidiStream = stream::impl::BidiStream<DefaultStreamTypeConfig>;

        using Config = context::Config;

        using context::use_default;

        using StreamID = stream::StreamID;
        using PacketNumber = packetnum::Value;

        template <class TypeConfig, class Lock = typename TypeConfig::recv_stream_lock>
        std::shared_ptr<stream::impl::RecvSorted<Lock>> set_stream_reader(stream::impl::RecvUniStream<TypeConfig>& r) {
            auto read = std::allocate_shared<stream::impl::RecvSorted<Lock>>(glheap_allocator<stream::impl::RecvSorted<Lock>>{});
            r.set_receiver(read, stream::impl::reader_recv_handler<Lock>);
            return read;
        }

    }  // namespace fnet::quic::use
}  // namespace utils

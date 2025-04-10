/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic - QUIC implemetation
// this header provides useful aliases of type that provides QUIC protocol context
// to customize configuration, see also context/context.h
// using namespace futils::fnet::quic::use to export usable alias
#pragma once
#include "context/context.h"
#include "context/default.h"
#include "context/typeconfig.h"
#include "stream/impl/recv_stream.h"
#include "stream/impl/set_receiver.h"
#include <mutex>

namespace futils {
    namespace fnet::quic::use {

        using Config = context::Config;

        using context::use_default_config;
        using context::use_default_context;

        using StreamID = stream::StreamID;
        using PacketNumber = packetnum::Value;

        using Reader = stream::impl::RecvSorter<std::mutex>;

        using FlowLimiter = stream::core::Limiter;

        using AppError = quic::AppError;

        // using smart (std::shared_ptr<void>) pointer for open/accept callback argument object
        namespace smartptr {
            using namespace use;
            using DefaultTypeConfig = context::TypeConfig<
                std::mutex,
                stream::TypeConfig<
                    std::mutex,
                    stream::UserStreamHandlerType<
                        stream::impl::SendBuffer<>,
                        stream::impl::FragmentSaver<std::shared_ptr<stream::impl::RecvSorter<std::mutex>>>>>>;
            using DefaultStreamTypeConfig = typename DefaultTypeConfig::stream_type_config;
            using Context = context::Context<DefaultTypeConfig>;

            using Streams = stream::impl::Conn<DefaultStreamTypeConfig>;
            using RecvStream = stream::impl::RecvUniStream<DefaultStreamTypeConfig>;
            using SendStream = stream::impl::SendUniStream<DefaultStreamTypeConfig>;
            using BidiStream = stream::impl::BidiStream<DefaultStreamTypeConfig>;

            constexpr auto sizeof_quic_Context = sizeof(Context);
            constexpr auto sizeof_quic_stream_Conn = sizeof(Streams);

            template <bool auto_update = true>
            std::shared_ptr<Context> make_quic(tls::TLSConfig&& conf, auto&& accept_callback, log::ConnLogger logger = {}) {
                auto ctx = use_default_context<DefaultTypeConfig>(std::move(conf), logger);
                auto h = ctx->get_streams()->get_conn_handler();
                using AcceptT = std::decay_t<decltype(accept_callback)>;
                h->set_auto_increase(true, true, true);
                auto cb = std::allocate_shared<AcceptT>(glheap_allocator<AcceptT>(), std::move(accept_callback));
                h->set_arg(std::move(cb));
                h->set_open_bidi([](std::shared_ptr<void>& arg, std::shared_ptr<BidiStream> stream) {
                    stream::impl::set_stream_reader(stream->receiver, auto_update);
                });
                h->set_accept_bidi([](std::shared_ptr<void>& arg, std::shared_ptr<BidiStream> stream) {
                    auto cb = static_cast<AcceptT*>(arg.get());
                    stream::impl::set_stream_reader(stream->receiver, auto_update);
                    (*cb)(stream, stream::StreamType::bidi);
                });
                h->set_accept_uni([](std::shared_ptr<void>& arg, std::shared_ptr<RecvStream> stream) {
                    auto cb = static_cast<AcceptT*>(arg.get());
                    stream::impl::set_stream_reader(*stream, auto_update);
                    (*cb)(stream, stream::StreamType::uni);
                });
                return ctx;
            }
        }  // namespace smartptr

        // using raw (void*) pointer for open/accept callback argument object
        // but for usability using smart_ptr (std::shared_ptr<void>) for receiver callback
        namespace rawptr {
            using namespace use;
            using DefaultStreamTypeConfig =
                stream::TypeConfig<
                    std::mutex,
                    stream::UserStreamHandlerType<
                        stream::impl::SendBuffer<>,
                        stream::impl::FragmentSaver<std::shared_ptr<stream::impl::RecvSorter<std::mutex>>>>,
                    stream::impl::Bind<stream::impl::ConnHandlerSingleArg, void*>::template bound>;
            using DefaultTypeConfig = context::TypeConfig<std::mutex, DefaultStreamTypeConfig>;
            using Context = context::Context<DefaultTypeConfig>;
            using Streams = stream::impl::Conn<DefaultStreamTypeConfig>;

            constexpr auto sizeof_quic_Context = sizeof(Context);
            constexpr auto sizeof_quic_stream_Conn = sizeof(Streams);

            using RecvStream = stream::impl::RecvUniStream<DefaultStreamTypeConfig>;
            using SendStream = stream::impl::SendUniStream<DefaultStreamTypeConfig>;
            using BidiStream = stream::impl::BidiStream<DefaultStreamTypeConfig>;

        }  // namespace rawptr

    }  // namespace fnet::quic::use
}  // namespace futils

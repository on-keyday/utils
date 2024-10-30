/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/server/servcpp.h>
#include <fnet/server/state.h>
#include <fnet/http3/http3.h>
#include <fnet/http3/mux.h>
#include <fnet/server/httpserv.h>
#include <thread>

namespace futils::fnet::server {

    static QUICServerState* get_quic_state(std::shared_ptr<void>& app_ctx) {
        return static_cast<QUICServerState*>(app_ctx.get());
    }

    void State::add_quic_thread(Socket&& listener, std::shared_ptr<quic_handler> handler,
                                fnet::quic::context::Config&& conf,
                                quic_server_config serv_conf) {
        auto internal = std::allocate_shared<QUICServerState>(glheap_allocator<QUICServerState>());
        internal->original_config = std::move(serv_conf);
        serv_conf.app_ctx = internal;
        internal->handler = handler;
        internal->state = shared_from_this();
        serv_conf.init_recv_stream = [](std::shared_ptr<void>& app, futils::fnet::quic::use::smartptr::RecvStream& stream) {
            futils::fnet::quic::stream::impl::set_stream_reader(stream, true, quic_handler::stream_recv_notify_callback);
            if (auto s = get_quic_state(app); s->original_config.init_recv_stream) {
                s->original_config.init_recv_stream(app, stream);
            }
        };
        serv_conf.init_send_stream = [](std::shared_ptr<void>& app, futils::fnet::quic::use::smartptr::SendStream& stream) {
            futils::fnet::quic::stream::impl::set_on_send_callback(stream, quic_handler::stream_send_notify_callback);
            if (auto s = get_quic_state(app); s->original_config.init_send_stream) {
                s->original_config.init_send_stream(app, stream);
            }
        };
        handler->set_server_config(std::move(conf), std::move(serv_conf));
        auto sock = std::move(listener);
        auto state = shared_from_this();
        std::thread(quic_send_thread, state, sock.clone(), handler).detach();
        std::thread(quic_send_scheduler, state, handler).detach();
        std::thread(quic_recv_scheduler, state, handler).detach();

        // this is async so it will not block
        quic_recv_handler(state, std::move(sock), handler);

        check_and_start();  // start GetQueuedCompletionStatus/epoll_wait thread
    }

    void State::quic_send_thread(std::shared_ptr<State> state, fnet::Socket sock, std::shared_ptr<quic_handler> handler) {
        byte buf[65536];
        while (!state->state().end_flag.test()) {
            auto [packet, addr, exist] = handler->create_packet(view::wvec(buf), true);
            if (!exist) {
                std::this_thread::yield();
                continue;
            }
            auto res = sock.writeto(addr, packet);
            if (!res) {
                state->log(log_level::err, &addr, res.error());
            }
        }
    }

    void State::quic_recv_handler(std::shared_ptr<State> state, fnet::Socket sock, std::shared_ptr<quic_handler> handler) {
        while (true) {
            auto res = sock.readfrom_async_deferred(fnet::async_notify_addr_then(
                BufferManager<byte[65536]>{},
                [state](fnet::DeferredCallback&& cb) { state->enqueue_callback(std::move(cb)); },
                [=](fnet::Socket&& sock, fnet::BufferManager<byte[65536]>& mgr, fnet::NetAddrPort&& addr, fnet::NotifyResult&& r) {
                    state->count.waiting_async_read--;
                    auto expected = r.readfrom_unwrap(mgr.get_buffer(), addr, [&](view::wvec buf) {
                        return sock.readfrom(buf);
                    });
                    if (!expected) {
                        error::Error err = error::ErrList{
                            .err = expected.error(),
                            .before = error::Error("failed to read from udp socket", error::Category::app),
                        };
                        state->log(log_level::err, &addr, err);
                        return;
                    }
                    auto msg = error::Error("received udp packet", error::Category::app);
                    state->log(log_level::debug, &expected->second, msg);
                    handler->parse_udp_payload(expected->first, expected->second);

                    sock.readfrom_until_block(mgr.get_buffer(), [&](view::wvec data, fnet::NetAddrPort addr) {
                        state->log(log_level::debug, &addr, msg);
                        handler->parse_udp_payload(data, addr);
                    });

                    quic_recv_handler(state, std::move(sock), handler);
                }));
            if (!res) {
                state->count.total_failed_read_async++;
                error::Error err = error::ErrList{
                    .err = res.error(),
                    .before = error::Error("failed to call async read from udp socket", error::Category::app),
                };
                state->log(log_level::err, nullptr, err);
                std::this_thread::yield();
                continue;
            }
            state->count.total_async_read_invocation++;
            auto concur = ++state->count.waiting_async_read - 1;
            state->count.max_concurrent_async_read_invocation.compare_exchange_strong(concur, concur + 1);
            break;
        }
    }

    void State::quic_send_scheduler(std::shared_ptr<State> state, std::shared_ptr<quic_handler> handler) {
        while (!state->state().end_flag.test()) {
            handler->schedule_send();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void State::quic_recv_scheduler(std::shared_ptr<State> state, std::shared_ptr<quic_handler> handler) {
        while (!state->state().end_flag.test()) {
            handler->schedule_recv(true);
        }
    }

    struct MakeStateContext {
        StateContext operator()(std::shared_ptr<State> state) const {
            return StateContext{state};
        }
    };

    constexpr auto make_state_context = MakeStateContext{};

    fnetserv_dll_internal(void) http3_server_setup(quic::server::MultiplexerConfig<quic::use::smartptr::DefaultTypeConfig>& c) {
        using QpackConfig = qpack::TypeConfig<flex_storage>;
        using H3Conf = http3::TypeConfig<QpackConfig, quic::use::smartptr::DefaultStreamTypeConfig>;
        using H3Conn = http3::stream::Connection<H3Conf>;
        using H3Stream = http3::stream::RequestStream<H3Conf>;
        struct InternalRequester {
            Requester req;
            H3Stream stream;
        };
        c.on_transport_parameter_read = http3::multiplexer_on_transport_parameter_read<QpackConfig, quic::use::smartptr::DefaultTypeConfig>;
        c.on_uni_stream_recv = http3::multiplexer_recv_uni_stream<QpackConfig, quic::use::smartptr::DefaultTypeConfig>;
        c.on_bidi_stream_recv = [](std::shared_ptr<void>& app, std::shared_ptr<futils::fnet::quic::use::smartptr::Context>&& ctx, std::shared_ptr<quic::use::smartptr::BidiStream>&& stream) {
            auto h3 = std::static_pointer_cast<H3Conn>(ctx->get_application_context());
            auto q_state = std::static_pointer_cast<QUICServerState>(app);
            auto ptr = std::static_pointer_cast<InternalRequester>(stream->receiver.get_or_set_application_context([&](std::shared_ptr<void>&& s, auto&& setter) {
                if (s) {
                    return s;
                }
                auto req = std::make_shared<InternalRequester>();
                req->stream.conn = h3;
                req->stream.stream = stream;
                req->stream.state = http3::stream::StateMachine::server_start;
                req->req.addr = q_state->handler.lock()->get_peer_addr(ctx->get_active_path());
                req->req.http = HTTP(http3::HTTP3(std::shared_ptr<H3Stream>(req, &req->stream)));
                setter(req);
                return std::static_pointer_cast<void>(req);
            }));
            auto s = q_state->state.lock();
            if (!s) {
                return;
            }
            auto requester = std::shared_ptr<Requester>(ptr, &ptr->req);
            HTTPServ* serv = (HTTPServ*)s->get_ctx();
            serv->next(serv->c, std::move(requester), make_state_context(s));
        };
    }

}  // namespace futils::fnet::server

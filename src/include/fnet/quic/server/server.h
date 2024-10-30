/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/hash_map.h"
#include "../../std/set.h"
#include "../hash_fn.h"
#include "../../storage.h"
#include "../context/context.h"
#include <tuple>
#include <queue>
#include "../path/addrmanage.h"
#include <thread/concurrent_queue.h>
#include "../stream/impl/set_receiver.h"
#include "../stream/impl/set_sender.h"

namespace futils {
    namespace fnet::quic::server {

        struct Handler {
            std::atomic<std::int64_t> next_deadline = -1;

            // returns (payload,idle)
            virtual std::tuple<view::rvec, path::PathID, bool> create_packet(context::maybe_resizable_buffer buf) = 0;

            // returns (should_send)
            virtual bool handle_packet(view::wvec payload, path::PathID pid) = 0;

            virtual std::shared_ptr<Handler> next_handler() {
                return nullptr;
            }
        };

        using HandlerPtr = std::shared_ptr<Handler>;

        template <class Lock>
        struct ConnIDMap {
            slib::hash_map<flex_storage, HandlerPtr> connid_maps;
            Lock connId_map_lock;

            void add_connID(view::rvec id, HandlerPtr&& ptr) {
                const auto l = helper::lock(connId_map_lock);
                connid_maps.emplace(flex_storage(id), ptr);
            }

            void replace_ids(auto&& ids, HandlerPtr&& ptr) {
                const auto l = helper::lock(connId_map_lock);
                for (auto& id : ids) {
                    connid_maps.erase(flex_storage(id.id));
                }
                for (auto& id : ids) {
                    connid_maps.emplace(flex_storage(id.id), ptr);
                }
            }

            void remove_ids(auto&& ids) {
                const auto l = helper::lock(connId_map_lock);
                for (auto& id : ids) {
                    connid_maps.erase(flex_storage(id.id));
                }
            }

            void remove_connID(view::rvec id) {
                const auto l = helper::lock(connId_map_lock);
                connid_maps.erase(flex_storage(id));
            }

            HandlerPtr lookup(view::rvec id) {
                const auto l = helper::lock(connId_map_lock);
                auto found = connid_maps.find(id);
                if (found != connid_maps.end()) {
                    return found->second;
                }
                return nullptr;
            }
        };

        template <class TConfig>
        struct HandlerMap;

        template <class Lock>
        struct Closed : Handler {
            close::ClosedContext ctx;
            slib::vector<connid::CloseID> ids;
            std::weak_ptr<ConnIDMap<Lock>> connid_map;
            // returns (payload,idle)
            std::tuple<view::rvec, path::PathID, bool> create_packet(context::maybe_resizable_buffer) override {
                next_deadline = ctx.close_timeout.get_deadline();
                auto [payload, idle] = ctx.create_udp_payload();
                return {
                    payload,
                    ctx.active_path,
                    idle,
                };
            }

            bool handle_packet(view::wvec payload, path::PathID pid) override {
                ctx.parse_udp_payload(payload);
                return false;
            }

            std::shared_ptr<Handler> next_handler() override {
                if (auto conn_id = connid_map.lock()) {
                    conn_id->remove_ids(ids);
                }
                return nullptr;
            }
        };

        template <class TConfig>
        struct Opened : Handler {
            using Ctx = context::Context<TConfig>;
            using Lock = typename TConfig::context_lock;
            Ctx ctx;
            bool closed = false;
            bool discard = false;
            Lock l;

            // returns (payload,idle)
            std::tuple<view::rvec, path::PathID, bool> create_packet(context::maybe_resizable_buffer buf) override {
                const auto d = helper::lock(l);
                auto v = ctx.create_udp_payload(buf);
                next_deadline = ctx.get_earliest_deadline();
                if (ctx.is_closed()) {
                    closed = true;
                    return {{}, path::original_path, false};  // transfer to ClosedContext
                }
                return v;
            }

            bool handle_packet(view::wvec payload, path::PathID id) override {
                const auto d = helper::lock(l);
                if (discard) {
                    return false;
                }
                ctx.parse_udp_payload(
                    payload, id,
                    {
                        .ctx = this,
                        .on_transport_parameter_read = [](void* ctx) {
                            auto obj = static_cast<Opened*>(ctx);
                            if (auto mux = obj->ctx.get_multiplexer_ptr().lock()) {
                                auto ptr = std::static_pointer_cast<HandlerMap<TConfig>>(mux);
                                auto obj_ptr = std::static_pointer_cast<Opened<TConfig>>(obj->ctx.get_outer_self_ptr().lock());
                                ptr->on_transport_parameter_read(std::shared_ptr<context::Context<TConfig>>(obj_ptr, &obj_ptr->ctx));
                            }
                        },
                    });
                next_deadline = ctx.get_earliest_deadline();
                return true;
            }

            std::shared_ptr<Handler> next_handler() override {
                const auto d = helper::lock(l);
                if (!closed || discard) {
                    return nullptr;
                }
                close::ClosedContext c;
                slib::vector<connid::CloseID> ids;
                ctx.expose_closed_context(c, ids);
                discard = true;
                auto next = std::allocate_shared<Closed<Lock>>(glheap_allocator<Closed<Lock>>{});
                next->ids = std::move(ids);
                next->ctx = std::move(c);
                if (auto mux = this->ctx.get_multiplexer_ptr().lock()) {
                    auto ptr = std::static_pointer_cast<HandlerMap<TConfig>>(mux);
                    auto conn_id = std::shared_ptr<ConnIDMap<Lock>>(ptr, &ptr->conn_ids);
                    conn_id->replace_ids(next->ids, next);
                    next->connid_map = conn_id;
                    auto obj_ptr = std::static_pointer_cast<Opened<TConfig>>(this->ctx.get_outer_self_ptr().lock());
                    ptr->close_connection(std::shared_ptr<context::Context<TConfig>>(obj_ptr, &obj_ptr->ctx));
                }
                ctx.release_resource();
                next->next_deadline = c.close_timeout.get_deadline();
                return next;
            }

            static std::shared_ptr<Opened> create() {
                return std::allocate_shared<Opened<TConfig>>(glheap_allocator<Opened<TConfig>>{});
            }
        };

        struct ZeroRTTBuffer : Handler {
            storage src_conn_id;
            storage dst_conn_id;
            std::tuple<view::rvec, path::PathID, bool> create_packet(context::maybe_resizable_buffer) override {
                return {{}, path::original_path, true};
            }

            bool handle_packet(view::wvec payload, path::PathID id) override {
                return false;
            }
        };

        template <class Lock>
        struct SenderQue {
            thread::MultiProduceSingleConsumeQueue<HandlerPtr, glheap_allocator<HandlerPtr>> queue;
            slib::set<HandlerPtr> sender_set;
            slib::set<HandlerPtr> duplicated;
            Lock l;

            auto lock() {
                l.lock();
                return helper::defer([&] {
                    l.unlock();
                });
            }

            void enque(const HandlerPtr& h, bool timeout) {
                const auto d = lock();
                if (sender_set.contains(h)) {
                    if (timeout) {
                        return;
                    }
                    duplicated.emplace(h);
                    return;  // duplicated, nothing to do
                }
                queue.push(h);
                sender_set.emplace(h);
            }

            HandlerPtr deque(bool block = false) {
                auto handler = block ? queue.pop_wait() : queue.pop();
                if (!handler) {
                    return nullptr;
                }
                const auto d = lock();
                duplicated.erase(*handler);
                return *handler;
            }

            void erase(const HandlerPtr& h) {
                const auto d = lock();
                sender_set.erase(h);
                duplicated.erase(h);
            }

            void erase_and_enque_if_duplicated(const HandlerPtr& h) {
                const auto d = lock();
                if (duplicated.contains(h)) {
                    queue.push(h);
                    duplicated.erase(h);
                    return;
                }
                sender_set.erase(h);
            }
        };

        template <class Lock>
        struct HandlerList {
            Lock l;
            slib::set<HandlerPtr> handlers;

            void add(HandlerPtr&& h) {
                const auto locked = helper::lock(l);
                handlers.insert(std::move(h));
            }

            void remove(HandlerPtr& rem) {
                const auto locked = helper::lock(l);
                handlers.erase(rem);
            }

            template <class L>
            void schedule(time::Clock& clock, SenderQue<L>& s) {
                const auto locked = helper::lock(l);
                auto now = clock.now();
                for (auto it = handlers.begin(); it != handlers.end(); it++) {
                    Handler* ptr = it->get();
                    if (now >= ptr->next_deadline) {
                        s.enque(*it, true);
                    }
                }
                return;
            }
        };

        template <class TConfig>
        struct MultiplexerConfig {
            QUIC_ctx_type(StreamTypeConfig, TConfig, stream_type_config);
            using BidiStream = stream::impl::BidiStream<StreamTypeConfig>;
            using RecvUniStream = stream::impl::RecvUniStream<StreamTypeConfig>;
            using SendUniStream = stream::impl::SendUniStream<StreamTypeConfig>;
            std::shared_ptr<void> app_ctx = nullptr;

            void (*prepare_connection)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&) = nullptr;
            void (*init_recv_stream)(std::shared_ptr<void>&, RecvUniStream&) = nullptr;
            void (*init_send_stream)(std::shared_ptr<void>&, SendUniStream&) = nullptr;
            void (*accept_uni_stream)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&, std::shared_ptr<RecvUniStream>&&) = nullptr;
            void (*accept_bidi_stream)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&, std::shared_ptr<BidiStream>&&) = nullptr;
            void (*start_connection)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&) = nullptr;
            // when transport parameter read, user may send data to peer
            void (*on_transport_parameter_read)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&) = nullptr;
            void (*close_connection)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&) = nullptr;

            // notify when stream received
            void (*on_bidi_stream_recv)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&, std::shared_ptr<BidiStream>&&) = nullptr;
            void (*on_uni_stream_recv)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&, std::shared_ptr<RecvUniStream>&&) = nullptr;
            void (*on_datagram_recv)(std::shared_ptr<void>&, std::shared_ptr<context::Context<TConfig>>&&) = nullptr;
        };

        template <class TConfig>
        struct RecvStreamNotify {
            std::shared_ptr<Opened<TConfig>> handler;
            stream::StreamID id;
            std::shared_ptr<void> stream;
        };

        template <class TConfig>
        struct HandlerMap : std::enable_shared_from_this<HandlerMap<TConfig>> {
            // std::uint64_t id = 0;
            using Lock = typename TConfig::context_lock;

           private:
            QUIC_ctx_type(StreamTypeConfig, TConfig, stream_type_config);
            QUIC_ctx_vector_type(ConnHandlerImpl, StreamTypeConfig, conn_handler);
            using ConnHandler = ConnHandlerImpl<StreamTypeConfig>;
            using BidiStream = stream::impl::BidiStream<StreamTypeConfig>;
            using RecvUniStream = stream::impl::RecvUniStream<StreamTypeConfig>;
            using SendUniStream = stream::impl::SendUniStream<StreamTypeConfig>;

            friend struct Opened<TConfig>;

            friend struct Closed<Lock>;

            HandlerList<Lock> handlers;
            ConnIDMap<Lock> conn_ids;
            SenderQue<Lock> send_que;
            path::ip::PathMapper paths;
            Lock path_lock;
            thread::MultiProduceSingleConsumeQueue<RecvStreamNotify<TConfig>, glheap_allocator<RecvStreamNotify<TConfig>>> recv_notify;

           public:
            path::PathID get_path_id(const NetAddrPort& addr) {
                const auto l = helper::lock(path_lock);
                return paths.get_path_id(addr);
            }

            NetAddrPort get_peer_addr(path::PathID pid) {
                const auto l = helper::lock(path_lock);
                return paths.get_peer_address(pid);
            }

           private:
            context::Config config;
            connid::ExporterFn exporter_fn = {
                connid::default_generator,
                +[](std::shared_ptr<void> pmp, std::shared_ptr<void> pop, view::rvec id, view::rvec sr) {
                    if (!pmp || !pop) {
                        return;
                    }
                    auto mp = std::static_pointer_cast<HandlerMap<TConfig>>(pmp);
                    auto ptr = std::static_pointer_cast<Opened<TConfig>>(pop);
                    mp->conn_ids.add_connID(id, std::move(ptr));
                },
                +[](std::shared_ptr<void> pmp, std::shared_ptr<void> pop, view::rvec id, view::rvec sr) {
                    if (!pmp || !pop) {
                        return;
                    }
                    auto mp = std::static_pointer_cast<HandlerMap<TConfig>>(pmp);
                    mp->conn_ids.remove_connID(id);
                },
            };

            bool is_supported(std::uint32_t ver) const {
                return config.version == ver;
            }

            void setup_server_config() {
                config.server = true;
                config.connid_parameters.exporter.exporter = &this->exporter_fn;
                config.connid_parameters.exporter.mux = this->weak_from_this();
            }

            context::Config get_config(std::shared_ptr<Opened<TConfig>> ptr, path::PathID id) {
                auto copy = config;
                copy.connid_parameters.exporter.obj = std::move(ptr);
                copy.path_parameters.original_path = id;
                return copy;
            }

            MultiplexerConfig<TConfig> server_config;
            MultiplexerConfig<TConfig> client_config;

            void init_recv_stream(RecvUniStream& stream, bool is_server) {
                MultiplexerConfig<TConfig>& config = is_server ? server_config : client_config;
                if (config.init_recv_stream) {
                    config.init_recv_stream(config.app_ctx, stream);
                }
            }

            void init_send_stream(SendUniStream& stream, bool is_server) {
                MultiplexerConfig<TConfig>& config = is_server ? server_config : client_config;
                if (config.init_send_stream) {
                    config.init_send_stream(config.app_ctx, stream);
                }
            }

            void accept_uni_stream(std::shared_ptr<context::Context<TConfig>>&& ctx, std::shared_ptr<RecvUniStream>&& stream) {
                MultiplexerConfig<TConfig>& config = ctx->is_server() ? server_config : client_config;
                if (config.accept_uni_stream) {
                    config.accept_uni_stream(config.app_ctx, std::move(ctx), std::move(stream));
                }
            }

            void accept_bidi_stream(std::shared_ptr<context::Context<TConfig>>&& ctx, std::shared_ptr<BidiStream>&& stream) {
                MultiplexerConfig<TConfig>& config = ctx->is_server() ? server_config : client_config;
                if (config.accept_bidi_stream) {
                    config.accept_bidi_stream(config.app_ctx, std::move(ctx), std::move(stream));
                }
            }

            void prepare_connection(const std::shared_ptr<context::Context<TConfig>>& ctx) {
                MultiplexerConfig<TConfig>& config = ctx->is_server() ? server_config : client_config;
                if (config.prepare_connection) {
                    auto copy = ctx;
                    config.prepare_connection(config.app_ctx, std::move(copy));
                }
            }

            void start_connection(std::shared_ptr<context::Context<TConfig>>&& ctx) {
                MultiplexerConfig<TConfig>& config = ctx->is_server() ? server_config : client_config;
                if (config.start_connection) {
                    config.start_connection(config.app_ctx, std::move(ctx));
                }
            }

            void on_transport_parameter_read(std::shared_ptr<context::Context<TConfig>>&& ctx) {
                MultiplexerConfig<TConfig>& config = ctx->is_server() ? server_config : client_config;
                if (config.on_transport_parameter_read) {
                    config.on_transport_parameter_read(config.app_ctx, std::move(ctx));
                }
            }

            void close_connection(std::shared_ptr<context::Context<TConfig>>&& ctx) {
                MultiplexerConfig<TConfig>& config = ctx->is_server() ? server_config : client_config;
                if (config.close_connection) {
                    config.close_connection(config.app_ctx, std::move(ctx));
                }
            }

            void set_stream_multiplexer_settings(std::shared_ptr<context::Context<TConfig>>& rel) {
                ConnHandler* handler = rel->get_streams()->get_conn_handler();
                handler->set_arg(std::static_pointer_cast<void>(rel));
                handler->set_open_bidi(+[](std::shared_ptr<void>& arg, std::shared_ptr<BidiStream> stream) {
                    auto ptr = std::static_pointer_cast<context::Context<TConfig>>(arg);
                    auto multiplexer = std::static_pointer_cast<HandlerMap<TConfig>>(ptr->get_multiplexer_ptr().lock());
                    multiplexer->init_recv_stream(stream->receiver, ptr->is_server());
                    multiplexer->init_send_stream(stream->sender, ptr->is_server());
                });
                handler->set_accept_bidi(+[](std::shared_ptr<void>& arg, std::shared_ptr<BidiStream> stream) {
                    auto ptr = std::static_pointer_cast<context::Context<TConfig>>(arg);
                    auto multiplexer = std::static_pointer_cast<HandlerMap<TConfig>>(ptr->get_multiplexer_ptr().lock());
                    multiplexer->init_recv_stream(stream->receiver, ptr->is_server());
                    multiplexer->init_send_stream(stream->sender, ptr->is_server());
                    multiplexer->accept_bidi_stream(std::move(ptr), std::move(stream));
                });
                handler->set_open_uni(+[](std::shared_ptr<void>& arg, std::shared_ptr<SendUniStream> stream) {
                    auto ptr = std::static_pointer_cast<context::Context<TConfig>>(arg);
                    auto multiplexer = std::static_pointer_cast<HandlerMap<TConfig>>(ptr->get_multiplexer_ptr().lock());
                    multiplexer->init_send_stream(*stream, ptr->is_server());
                });
                handler->set_accept_uni(+[](std::shared_ptr<void>& arg, std::shared_ptr<RecvUniStream> stream) {
                    auto ptr = std::static_pointer_cast<context::Context<TConfig>>(arg);
                    auto multiplexer = std::static_pointer_cast<HandlerMap<TConfig>>(ptr->get_multiplexer_ptr().lock());
                    multiplexer->init_recv_stream(*stream, ptr->is_server());
                    multiplexer->accept_uni_stream(std::move(ptr), std::move(stream));
                });
            }

            // open client side
            std::shared_ptr<Opened<TConfig>> open_internal(context::Config conf, const NetAddrPort& dest, const char* host) {
                std::shared_ptr<Opened<TConfig>> opened = Opened<TConfig>::create();
                Opened<TConfig>* ptr = opened.get();
                auto path_id = get_path_id(dest);
                conf.server = false;
                conf.connid_parameters.exporter.exporter = &this->exporter_fn;
                conf.connid_parameters.exporter.mux = this->weak_from_this();
                conf.connid_parameters.exporter.obj = opened;
                conf.path_parameters.original_path = path_id;
                if (!ptr->ctx.init(conf)) {
                    conf.logger.report_error(ptr->ctx.get_conn_error());
                    return nullptr;
                }
                auto rel = std::shared_ptr<context::Context<TConfig>>(opened, &ptr->ctx);
                set_stream_multiplexer_settings(rel);
                this->prepare_connection(rel);
                {
                    const auto l = helper::lock(ptr->l);
                    if (!ptr->ctx.connect_start(host)) {
                        return nullptr;  // failed to connect
                    }
                    this->start_connection(std::move(rel));
                }
                send_que.enque(opened, false);
                handlers.add(opened);
                return opened;
            }

            void add_new_conn(view::rvec origDst, view::wvec d, path::PathID pid) {
                std::shared_ptr<Opened<TConfig>> opened = Opened<TConfig>::create();
                Opened<TConfig>* ptr = opened.get();
                auto config = get_config(opened, pid);
                if (!ptr->ctx.init(config)) {
                    config.logger.report_error(ptr->ctx.get_conn_error());
                    return;  // failed to init
                }
                auto rel = std::shared_ptr<context::Context<TConfig>>(opened, &ptr->ctx);
                set_stream_multiplexer_settings(rel);
                this->prepare_connection(rel);
                {
                    const auto l = helper::lock(ptr->l);
                    if (!ptr->ctx.accept_start(origDst)) {
                        return;  // failed to accept
                    }
                    this->start_connection(std::move(rel));
                    ptr->ctx.parse_udp_payload(d, pid);
                }
                send_que.enque(opened, false);
                handlers.add(opened);
            }

            void opening_packet(packet::PacketSummary summary, view::wvec d, path::PathID pid) {
                if (summary.type != PacketType::Initial) {
                    if (summary.type == PacketType::ZeroRTT) {
                    }
                    return;
                }
                if (d.size() < path::initial_udp_datagram_size) {
                    return;  // ignore smaller packet
                }
                // check versions
                if (!is_supported(summary.version)) {
                    return;
                }
                auto origDst = summary.dstID;
                bool token_is_valid = false;
                // TODO(on-keyday): write token validation
                return add_new_conn(origDst, d, pid);
            }

            void handle_packet(packet::PacketSummary summary, view::wvec d, path::PathID pid) {
                auto ptr = conn_ids.lookup(summary.dstID);
                if (!ptr) {
                    return opening_packet(summary, d, pid);
                }
                ptr->handle_packet(d, pid);
                send_que.enque(ptr, false);
            }

           public:
            void set_server_config(context::Config&& conf,
                                   MultiplexerConfig<TConfig> server_config = {}) {
                config = std::move(conf);
                this->server_config = std::move(server_config);
                setup_server_config();
            }

            void set_client_config(MultiplexerConfig<TConfig> client_config) {
                this->client_config = std::move(client_config);
            }

            std::shared_ptr<context::Context<TConfig>> open(const context::Config& conf, const NetAddrPort& dest, const char* host) {
                auto opened = open_internal(conf, dest, host);
                if (!opened) {
                    return nullptr;
                }
                return std::shared_ptr<context::Context<TConfig>>(opened, &opened->ctx);
            }

            void parse_udp_payload(view::wvec d, const NetAddrPort& addr) {
                auto path_id = get_path_id(addr);
                binary::reader r{d};
                auto cb = [&](PacketType, auto& p, view::rvec src, bool err, bool valid_type) {
                    if constexpr (std::is_same_v<decltype(p), packet::Packet&> ||
                                  std::is_same_v<decltype(p), packet::LongPacketBase&> ||
                                  std::is_same_v<decltype(p), packet::VersionNegotiationPacket<slib::vector>&> ||
                                  std::is_same_v<decltype(p), packet::StatelessReset&>) {
                        return false;
                    }
                    else {
                        return handle_packet(packet::summarize(p), d, path_id);
                    }
                };
                auto is_stateless_reset = [&](packet::StatelessReset& reset) {
                    return false;
                };
                auto get_dstID_len = [&](binary::reader r, size_t* len) {
                    *len = config.connid_parameters.connid_len;
                    return true;
                };
                auto res = packet::parse_packet<slib::vector>(r, crypto::authentication_tag_length, cb, is_stateless_reset, get_dstID_len);
                if (!res) {
                    return;
                }
            }

            // thread safe call
            // call by multiple thread not make error
            void notify(const std::shared_ptr<context::Context<TConfig>>& ctx) {
                auto ptr = ctx->get_outer_self_ptr().lock();
                if (!ptr) {
                    return;
                }
                send_que.enque(std::static_pointer_cast<Handler>(std::move(ptr)), false);
            }

            static void stream_send_notify_callback(std::shared_ptr<void>&& conn_ctx, stream::StreamID id) {
                auto ctx = std::static_pointer_cast<Opened<TConfig>>(std::move(conn_ctx));
                auto mux = std::static_pointer_cast<HandlerMap<TConfig>>(ctx->ctx.get_multiplexer_ptr().lock());
                mux->send_que.enque(std::static_pointer_cast<Handler>(std::move(ctx)), false);
            }

            static void stream_recv_notify_callback(std::shared_ptr<void>&& conn_ctx, stream::StreamID id) {
                auto ctx = std::static_pointer_cast<Opened<TConfig>>(std::move(conn_ctx));
                auto mux = std::static_pointer_cast<HandlerMap<TConfig>>(ctx->ctx.get_multiplexer_ptr().lock());
                std::shared_ptr<stream::impl::Conn<StreamTypeConfig>> streams = ctx->ctx.get_streams();
                stream::impl::Conn<StreamTypeConfig>* ptr = streams.get();
                // because of auto_remove mode, stream may removed from streams after this call
                // if stream is removed and if not acquired this stream here,
                // enqueued data will be lost and not sent notification correctly.
                // so acquire stream here
                // at here, stream referred by id is locked,
                // but streams object is not locked
                // so this doesn't make recursive lock and deadlock
                std::shared_ptr<void> stream;
                if (id.type() == stream::StreamType::bidi) {
                    stream = ptr->find_bidi(id);
                }
                else {
                    stream = ptr->find_recv_uni(id);
                }
                if (!stream) {
                    return;  // already removed stream
                }
                mux->recv_notify.push(RecvStreamNotify<TConfig>{std::move(ctx), id, std::move(stream)});
            }

            static void datagram_send_notify_callback(std::shared_ptr<void>&& conn_ctx) {
                auto ctx = std::static_pointer_cast<Opened<TConfig>>(std::move(conn_ctx));
                auto mux = std::static_pointer_cast<HandlerMap<TConfig>>(ctx->ctx.get_multiplexer_ptr().lock());
                mux->send_que.enque(std::static_pointer_cast<Handler>(std::move(ctx)), false);
            }

            static void datagram_recv_notify_callback(std::shared_ptr<void>&& conn_ctx) {
                auto ctx = std::static_pointer_cast<Opened<TConfig>>(std::move(conn_ctx));
                auto mux = std::static_pointer_cast<HandlerMap<TConfig>>(ctx->ctx.get_multiplexer_ptr().lock());
                mux->recv_notify.push(RecvStreamNotify<TConfig>{std::move(ctx), stream::invalid_id, nullptr});
            }

            void schedule_send() {
                handlers.schedule(config.internal_parameters.clock, send_que);
            }

            void schedule_recv(bool block = false) {
                while (true) {
                    std::optional<RecvStreamNotify<TConfig>> notify = block ? recv_notify.pop_wait() : recv_notify.pop();
                    if (!notify) {
                        break;
                    }
                    auto ctx = std::shared_ptr<context::Context<TConfig>>(notify->handler, &notify->handler->ctx);
                    if (!notify->id.valid()) {
                        if (server_config.on_datagram_recv) {
                            server_config.on_datagram_recv(server_config.app_ctx, std::move(ctx));
                        }
                    }
                    else {
                        if (notify->id.type() == stream::StreamType::bidi) {
                            if (server_config.on_bidi_stream_recv) {
                                server_config.on_bidi_stream_recv(server_config.app_ctx, std::move(ctx), std::static_pointer_cast<BidiStream>(std::move(notify->stream)));
                            }
                        }
                        else {
                            if (server_config.on_uni_stream_recv) {
                                server_config.on_uni_stream_recv(server_config.app_ctx, std::move(ctx), std::static_pointer_cast<RecvUniStream>(std::move(notify->stream)));
                            }
                        }
                    }
                }
            }

            // thread safe call
            // call by multiple thread not make error
            std::tuple<view::rvec, NetAddrPort, bool> create_packet(context::maybe_resizable_buffer buffer, bool block = false) {
                while (true) {
                    auto handler = send_que.deque(block);
                    if (!handler) {
                        return {{}, {}, false};
                    }
                    auto erase = helper::defer([&] {
                        send_que.erase(handler);
                    });
                    auto [payload, path_id, idle] = handler->create_packet(buffer);
                    if (!idle) {
                        auto next = handler->next_handler();
                        auto h = helper::defer([&] {
                            handlers.remove(handler);
                        });
                        if (!next) {
                            continue;  // drop handler
                        }
                        send_que.enque(std::move(next), true);
                        continue;
                    }
                    if (payload.size() == 0) {
                        erase.cancel();
                        send_que.erase_and_enque_if_duplicated(handler);
                        continue;
                    }
                    erase.execute();
                    send_que.enque(handler, false);
                    return {payload, get_peer_addr(path_id), true};
                }
            }
        };
    }  // namespace fnet::quic::server
}  // namespace futils

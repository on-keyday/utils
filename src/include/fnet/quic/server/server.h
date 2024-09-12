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

namespace futils {
    namespace fnet::quic::server {

        using BytesChannel = thread::MultiProducerChannelBuffer<storage, glheap_allocator<storage>>;

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

        struct Closed : Handler {
            close::ClosedContext ctx;
            slib::vector<connid::CloseID> ids;
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
                ctx.parse_udp_payload(payload, id);
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
                auto next = std::allocate_shared<Closed>(glheap_allocator<Closed>{});
                next->ids = std::move(ids);
                next->ctx = std::move(c);
                next->next_deadline = c.close_timeout.get_deadline();
                return next;
            }

            static std::shared_ptr<Opened> create() {
                std::shared_ptr<Opened<TConfig>> open = std::allocate_shared<Opened<TConfig>>(glheap_allocator<Opened<TConfig>>{});
                open->ctx.set_mux_ptr(open);
                return open;
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

        using HandlerPtr = std::shared_ptr<Handler>;

        template <class Lock>
        struct SenderQue {
            slib::deque<HandlerPtr> sender_que;
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
                sender_que.push_back(h);
                sender_set.emplace(h);
            }

            HandlerPtr deque() {
                const auto d = lock();
                if (sender_que.empty()) {
                    return {};
                }
                auto h = std::move(sender_que.front());
                sender_que.pop_front();
                duplicated.erase(h);
                return h;
            }

            void erase(const HandlerPtr& h) {
                const auto d = lock();
                sender_set.erase(h);
                duplicated.erase(h);
            }

            void erase_and_enque_if_duplicated(const HandlerPtr& h) {
                const auto d = lock();
                if (duplicated.contains(h)) {
                    sender_que.push_back(h);
                    duplicated.erase(h);
                    return;
                }
                sender_set.erase(h);
            }
        };

        template <class Lock>
        struct HandlerList {
            Lock l;
            slib::list<HandlerPtr> handlers;

            void add(HandlerPtr&& h) {
                const auto locked = helper::lock(l);
                handlers.push_back(std::move(h));
            }

            void remove(HandlerPtr& rem) {
                handlers.remove_if([&](auto&& v) {
                    return v == rem;
                });
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

        template <class Lock>
        struct ConnIDMap {
            slib::hash_map<flex_storage, HandlerPtr> connid_maps;
            Lock connId_map_lock;

            void add_connID(view::rvec id, HandlerPtr&& ptr) {
                const auto l = helper::lock(connId_map_lock);
                connid_maps.emplace(flex_storage(id), ptr);
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
        struct HandlerMap : std::enable_shared_from_this<HandlerMap<TConfig>> {
            std::uint64_t id = 0;
            using Lock = typename TConfig::context_lock;

            HandlerList<Lock> handlers;
            ConnIDMap<Lock> conn_ids;
            SenderQue<Lock> send_que;
            path::ip::PathMapper paths;
            Lock path_lock;

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
                +[](void* pmp, void* pop, view::rvec id, view::rvec sr) {
                    if (!pmp || !pop) {
                        return;
                    }
                    auto mp = static_cast<HandlerMap<TConfig>*>(pmp);
                    auto op = static_cast<context::Context<TConfig>*>(pop);
                    auto ptr = std::static_pointer_cast<Opened<TConfig>>(op->get_mux_ptr());
                    mp->conn_ids.add_connID(id, std::move(ptr));
                },
                +[](void* pmp, void* pop, view::rvec id, view::rvec sr) {
                    if (!pmp || !pop) {
                        return;
                    }
                    auto mp = static_cast<HandlerMap<TConfig>*>(pmp);
                    mp->conn_ids.remove_connID(id);
                },
            };

            bool is_supported(std::uint32_t ver) const {
                return config.version == ver;
            }

            void setup_config() {
                config.server = true;
                config.connid_parameters.exporter.exporter = &this->exporter_fn;
                config.connid_parameters.exporter.mux = this->weak_from_this();
            }

            context::Config get_config(std::shared_ptr<context::Context<TConfig>> ptr, path::PathID id) {
                auto copy = config;
                copy.connid_parameters.exporter.obj = std::move(ptr);
                copy.path_parameters.original_path = id;
                return config;
            }

           public:
            void set_config(context::Config&& conf) {
                config = std::move(conf);
                if (config.connid_parameters.exporter.exporter) {
                    exporter_fn.gen_callback = config.connid_parameters.exporter.exporter->gen_callback;
                }
                setup_config();
            }

            void add_new_conn(view::rvec origDst, view::wvec d, path::PathID pid) {
                std::shared_ptr<Opened<TConfig>> opened = Opened<TConfig>::create();
                Opened<TConfig>* ptr = opened.get();
                auto rel = std::shared_ptr<context::Context<TConfig>>(opened, &ptr->ctx);
                if (!ptr->ctx.init(get_config(rel, pid))) {
                    return;  // failed to init
                }
                {
                    const auto l = helper::lock(ptr->l);
                    if (!ptr->ctx.accept_start(origDst)) {
                        return;  // failed to accept
                    }
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
                auto ptr = ctx->get_mux_ptr();
                if (!ptr) {
                    return;
                }
                send_que.enque(std::static_pointer_cast<Handler>(std::move(ptr)));
            }

            void schedule() {
                handlers.schedule(config.internal_parameters.clock, send_que);
            }

            // thread safe call
            // call by multiple thread not make error
            std::tuple<view::rvec, NetAddrPort, bool> create_packet(context::maybe_resizable_buffer buffer) {
                while (true) {
                    auto handler = send_que.deque();
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

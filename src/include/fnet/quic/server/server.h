/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

namespace utils {
    namespace fnet::quic::server {
        struct Handler {
            time::Time next_deadline = time::invalid;

            // returns (payload,idle)
            virtual std::pair<view::rvec, bool> create_packet(flex_storage& buf) = 0;

            // returns (should_send)
            virtual bool handle_packet(view::wvec payload) = 0;

            virtual std::shared_ptr<Handler> next_handler() {
                return nullptr;
            }
        };

        struct Closed : Handler {
            close::ClosedContext ctx;
            slib::vector<connid::CloseID> ids;
            // returns (payload,idle)
            std::pair<view::rvec, bool> create_packet(flex_storage&) override {
                next_deadline = ctx.close_timeout.get_deadline();
                return ctx.create_udp_payload();
            }

            bool handle_packet(view::wvec payload) override {
                ctx.parse_udp_payload(payload);
                return false;
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

            auto lock() {
                l.lock();
                return helper::defer([&] {
                    l.unlock();
                });
            }

            // returns (payload,idle)
            std::pair<view::rvec, bool> create_packet(flex_storage& buf) override {
                const auto d = lock();
                auto v = ctx.create_udp_payload(&buf);
                next_deadline = ctx.get_earliest_deadline();
                if (ctx.is_closed()) {
                    closed = true;
                    return {{}, false};  // trnasfer to ClosedContext
                }
                return v;
            }

            bool handle_packet(view::wvec payload) override {
                const auto d = lock();
                if (discard) {
                    return;
                }
                ctx.parse_udp_payload(payload);
                next_deadline = ctx.get_earliest_deadline();
                return true;
            }

            std::shared_ptr<Handler> next_handler() override {
                const auto d = lock();
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
                next->next_deadline = c.close_timeout;
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
            std::pair<view::rvec, bool> create_packet(flex_storage& buf) override {
                return {{}, true};
            }

            bool handle_packet(view::wvec payload) override {
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

            void enque(const HandlerPtr& h) {
                const auto d = lock();
                if (sender_set.contains(h)) {
                    duplicated.emplace(h);
                    return;  // duplicated, nothing to do
                }
                sender_que.push_back(h);
                sender_set.emplace(h);
            }

            HandlerPtr deque() {
                const auto d = lock();
                if (sender_que.empty()) {
                    return nullptr;
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

        template <class TConfig>
        struct HandlerMap {
            std::uint64_t id = 0;

            slib::hash_map<std::uint64_t, HandlerPtr> handlers;
            slib::hash_map<flex_storage, HandlerPtr> connid_maps;
            SenderQue<typename TConfig::context_lock> send_que;

            void opening_packet(packet::PacketSummary summary, view::wvec d) {
                if (summary.type != PacketType::Initial) {
                    if (summary.type == PacketType::ZeroRTT) {
                    }
                }
                if (d.size() < path::initial_udp_datagram_size) {
                    return;  // ignore smaller packet
                }
            }

            void handle_packet(packet::PacketSummary summary, view::wvec d) {
                auto found = connid_maps.find(summary.dstID);
                if (found != connid_maps.end()) {
                    return opening_packet(summary, d);
                }
                HandlerPtr& ptr = found->second;
                ptr->handle_packet(d);
                send_que.enque(ptr);
            }

            void parse_udp_payload(view::wvec d) {
                io::reader r{d};
                auto cb = [&](PacketType, auto& p, view::rvec src, bool err, bool valid_type) {
                    if constexpr (std::is_same_v<decltype(p), packet::Packet&> ||
                                  std::is_same_v<decltype(p), packet::LongPacketBase&> ||
                                  std::is_same_v<decltype(p), packet::VersionNegotiationPacket<slib::vector>&> ||
                                  std::is_same_v<decltype(p), packet::StatelessReset&>) {
                        return false;
                    }
                    else {
                        return handle_packet(packet::summarize(p), d);
                    }
                };
                auto is_stateless_reset = [&](packet::StatelessReset& reset) {
                    return false;
                };
                auto get_dstID_len = [&](io::reader r, size_t* len) {
                    *len = 0;
                    return false;
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
                    return false;
                }
                send_que.enque(std::static_pointer_cast<Handler>(std::move(ptr)));
            }

            // thread safe call
            // call by multiple thread not make error
            std::pair<view::rvec, bool> create_packet(flex_storage& buffer) {
                while (true) {
                    auto handler = send_que.deque();
                    if (!handler) {
                        return {{}, false};
                    }
                    auto erase = helper::defer([&] {
                        send_que.erase(handler);
                    });
                    auto [payload, idle] = handler->create_packet(buffer);
                    if (!idle) {
                        auto next = handler->next_handler();
                        if (!next) {
                            continue;  // drop handler
                        }
                        send_que.enque(std::move(next));
                        continue;
                    }
                    if (payload.size() == 0) {
                        erase.cancel();
                        send_que.erase_and_enque_if_duplicated(handler);
                        continue;
                    }
                    erase.execute();
                    send_que.enque(handler);
                    return {payload, true};
                }
            }
        };
    }  // namespace fnet::quic::server
}  // namespace utils

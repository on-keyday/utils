/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/hash_map.h"
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
            virtual std::pair<view::rvec, bool> create_packet() = 0;

            virtual void handle_packet(view::wvec payload) = 0;

            virtual std::shared_ptr<Handler> next_handler() {
                return nullptr;
            }
        };

        struct Closed : Handler {
            close::ClosedContext ctx;
            slib::vector<connid::CloseID> ids;
            // returns (payload,idle)
            std::pair<view::rvec, bool> create_packet() override {
                next_deadline = ctx.close_timeout.get_deadline();
                return ctx.create_udp_payload();
            }

            void handle_packet(view::wvec payload) override {
                ctx.parse_udp_payload(payload);
            }
        };

        template <class TConfig>
        struct Opened : Handler {
            using Ctx = context::Context<TConfig>;
            Ctx ctx;
            bool closed = false;

            // returns (payload,idle)
            std::pair<view::rvec, bool> create_packet() override {
                auto v = ctx.create_udp_payload();
                next_deadline = ctx.get_earliest_deadline();
                return v;
            }

            void handle_packet(view::wvec payload) override {
                ctx.parse_udp_payload(payload);
                next_deadline = ctx.get_earliest_deadline();
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
            std::pair<view::rvec, bool> create_packet() override {
                return {{}, false};
            }

            void handle_packet(view::wvec payload) override {
            }
        };

        template <class TConfig>
        struct HandlerMap {
            using HandlerPtr = std::shared_ptr<Handler>;
            std::uint64_t id = 0;
            slib::hash_map<std::uint64_t, HandlerPtr> handlers;
            slib::hash_map<flex_storage, HandlerPtr> connid_maps;

            void opening_packet(packet::PacketSummary summary, view::wvec d) {
                if (summary.type != PacketType::Initial) {
                    if (summary.type == PacketType::ZeroRTT) {
                    }
                }
                if (d.size() < path::initial_udp_datagram_size_limit) {
                    return;  // ignore smaller packet
                }
            }

            void handle_packet(packet::PacketSummary summary, view::wvec d) {
                auto found = handlers.find(summary.dstID);
                if (found != handlers.end()) {
                    return opening_packet(summary, d);
                }
                HandlerPtr& ptr = found->second;
                ptr->handle_packet(d);
            }

            void parse_udp_payload(view::wvec d) {
                io::reader r{d};
                auto cb = [&](PacketType, auto& p, view::rvec src, bool err, bool valid_type) {
                    if constexpr (std::is_same_v<decltype(p), packet::Packet&> ||
                                  std::is_same_v<decltype(p), packet::LongPacketBase&> ||
                                  std::is_same_v<decltype(p), packet::VersionNegotiationPacket<slib::vector>>) {
                        return false;
                    }
                    else {
                        return handle_packet(p, d);
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

            void create_packets() {
                slib::list<HandlerPtr> tmp_saver;
                for (auto it = handlers.begin(); it != handlers.end();) {
                    auto [payload, idle] = it->second->create_packet();
                    if (!idle) {
                        auto next = it->second->next_handler();
                        handlers.erase(it);
                        if (next) {
                            tmp_saver.push_back(std::move(next));
                        }
                        continue;
                    }

                    it++;
                }
            }
        };
    }  // namespace fnet::quic::server
}  // namespace utils

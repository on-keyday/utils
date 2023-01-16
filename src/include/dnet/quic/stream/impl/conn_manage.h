/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn_manage - connection open close
#pragma once
#include "conn.h"
#include "stream.h"
#include "../../../std/hash_map.h"
#include "../../../std/list.h"

namespace utils {
    namespace dnet::quic::stream::impl {

        namespace internal {
            template <class T, class... Args>
            std::shared_ptr<T> make_ptr(Args&&... arg) {
                return std::allocate_shared<T>(glheap_allocator<T>{}, std::forward<Args>(arg)...);
            }
        }  // namespace internal

        template <class Lock>
        struct Conn : std::enable_shared_from_this<Conn<Lock>> {
            ConnFlowControl<Lock> control;
            slib::hash_map<StreamID, std::shared_ptr<SendUniStream<Lock>>> send_uni;
            slib::hash_map<StreamID, std::shared_ptr<BidiStream<Lock>>> send_bidi;
            slib::hash_map<StreamID, std::shared_ptr<RecvUniStream<Lock>>> recv_uni;
            slib::hash_map<StreamID, std::shared_ptr<BidiStream<Lock>>> recv_bidi;
            slib::list<std::shared_ptr<RecvUniStream<Lock>>> accept_wait_uni;
            slib::list<std::shared_ptr<BidiStream<Lock>>> accept_wait_bidi;

            Conn(Direction self) {
                control.base.state.set_dir(self);
            }

            void apply_initial_limits(const InitialLimits& local, const InitialLimits& peer) {
                control.base.set_initial_limits(local, peer);
            }

            auto borrow_control() {
                return std::shared_ptr<ConnFlowControl<Lock>>(
                    this->shared_from_this(), &control);
            }

            std::shared_ptr<SendUniStream<Lock>> open_uni() {
                std::shared_ptr<SendUniStream<Lock>> s;
                control.base.open_uni([&](StreamID id, bool blocked) {
                    if (blocked) {
                        return;
                    }
                    s = internal::make_ptr<SendUniStream<Lock>>(id, brrow_control());
                    send_uni.emplace(id, s);
                });
                return s;
            }

            std::shared_ptr<BidiStream<Lock>> open_bidi() {
                std::shared_ptr<BidiStream<Lock>> s;
                control.base.open_bidi([&](StreamID id, bool blocked) {
                    if (blocked) {
                        return;
                    }
                    s = std::allocate_shared<BidiStream<Lock>>(
                        glheap_allocator<BidiStream<Lock>>{},
                        id, borrow_control());
                    send_bidi.emplace(id, s);
                });
                return s;
            }

            Direction local_dir() const {
                return control.base.state.local_dir();
            }

            Direction peer_dir() const {
                return control.base.state.peer_dir();
            }

            error::Error check_creation(FrameType type, StreamID id) {
                if (auto [by_local, err] = control.base.check_recv_issued(type, id); by_local) {
                    return err;
                }
                error::Error oerr;
                control.base.accept(id, [&](StreamID id, bool, error::Error err) {
                    if (err) {
                        oerr = std::move(err);
                        return;
                    }
                    if (oerr) {
                        return;  // any way fail
                    }
                    if (id.type() == StreamType::bidi) {
                        auto s = internal::make_ptr<BidiStream<Lock>>(id, borrow_control());
                        auto [_, ok] = recv_bidi.try_emplace(id, s);
                        if (!ok) {
                            oerr = error::Error("unexpected cration failure on bidi stream!");
                            return;
                        }
                        accept_wait_bidi.push_back(s);
                    }
                    else {
                        auto s = internal::make_ptr<RecvUniStream<Lock>>(id, borrow_control());
                        auto [_, ok] = recv_uni.try_emplace(id, s);
                        if (!ok) {
                            oerr = error::Error("unexpected creation failure on uni stream!");
                            return;
                        }
                        accept_wait_uni.push_back(s);
                    }
                });
                return oerr;
            }

            std::shared_ptr<BidiStream<Lock>> accept_bidi() {
                const auto locked = control.base.accept_bidi_lock();
                if (accept_wait_bidi.empty()) {
                    return nullptr;
                }
                auto s = std::move(accept_wait_bidi.front());
                accept_wait_bidi.pop_front();
                return s;
            }

            std::shared_ptr<RecvUniStream<Lock>> accept_uni() {
                const auto locked = control.base.accept_uni_lock();
                if (accept_wait_uni.empty()) {
                    return nullptr;
                }
                auto s = std::move(accept_wait_uni.front());
                accept_wait_uni.pop_front();
                return s;
            }

            std::shared_ptr<BidiStream<Lock>> find_recv_bidi(StreamID id) {
                const auto locked = control.base.accept_bidi_lock();
                if (auto it = recv_bidi.find(id); it != recv_bidi.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<RecvUniStream<Lock>> find_recv_uni(StreamID id) {
                const auto locked = control.base.accept_uni_lock();
                if (auto it = recv_uni.find(id); it != recv_uni.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<BidiStream<Lock>> find_send_bidi(StreamID id) {
                const auto locked = control.base.open_bidi_lock();
                if (auto it = send_bidi.find(id); it != send_bidi.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<SendUniStream<Lock>> find_send_uni(StreamID id) {
                const auto locked = control.base.open_uni_lock();
                if (auto it = send_uni.find(id); it != send_uni.end()) {
                    return it->second;
                }
                return nullptr;
            }
        };

        template <class Lock>
        std::shared_ptr<Conn<Lock>> make_conn(Direction self) {
            return internal::make_ptr<Conn<Lock>>(self);
        }
    }  // namespace dnet::quic::stream::impl
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn_manage - connection open close
#pragma once
#include "conn_flow.h"
#include "stream.h"
#include "../../../std/hash_map.h"
#include "../../../std/list.h"
#include "../../hash_fn.h"
#include <atomic>

namespace utils {
    namespace fnet::quic::stream::impl {

        namespace internal {
            template <class T, class... Args>
            std::shared_ptr<T> make_ptr(Args&&... arg) {
                return std::allocate_shared<T>(glheap_allocator<T>{}, std::forward<Args>(arg)...);
            }
        }  // namespace internal

        struct SendeSchedArg {
            std::shared_ptr<void> unisendarg;
            std::shared_ptr<void> bidisendarg;
            std::shared_ptr<void> unirecvarg;
            std::shared_ptr<void> bidirecvarg;
        };

        template <class Lock>
        struct Conn : std::enable_shared_from_this<Conn<Lock>> {
            using UniOpenCB = void (*)(std::shared_ptr<void>& arg, std::shared_ptr<SendUniStream<Lock>> stream);
            using UniAcceptCB = void (*)(std::shared_ptr<void>& arg, std::shared_ptr<RecvUniStream<Lock>> stream);
            using BidiCB = void (*)(std::shared_ptr<void>& arg, std::shared_ptr<BidiStream<Lock>> stream);
            using SendCB = IOResult (*)(SendeSchedArg, frame::fwriter& w, ack::ACKCollector observer_vec);

           private:
            ConnFlowControl<Lock> control;
            slib::hash_map<StreamID, std::shared_ptr<SendUniStream<Lock>>> send_uni;
            slib::hash_map<StreamID, std::shared_ptr<BidiStream<Lock>>> send_bidi;
            slib::hash_map<StreamID, std::shared_ptr<RecvUniStream<Lock>>> recv_uni;
            slib::hash_map<StreamID, std::shared_ptr<BidiStream<Lock>>> recv_bidi;
            std::atomic_bool remove_auto = false;

            std::shared_ptr<void> unisendarg;
            std::shared_ptr<void> bidisendarg;
            UniOpenCB open_uni_stream = nullptr;
            BidiCB open_bidi_stream = nullptr;

            std::shared_ptr<void> unirecvarg;
            std::shared_ptr<void> bidirecvarg;
            UniAcceptCB accept_uni_stream = nullptr;
            BidiCB accept_bidi_stream = nullptr;

            SendCB send_schedule = nullptr;

            auto borrow_control() {
                return std::shared_ptr<ConnFlowControl<Lock>>(
                    this->shared_from_this(), &control);
            }

           public:
            Conn(Direction self) {
                control.base.state.set_dir(self);
            }

            void set_auto_remove(bool is_auto) {
                remove_auto = is_auto;
            }

            bool is_flow_control_limited() {
                bool res = false;
                control.base.use_send_credit([&](std::uint64_t avail_size, std::uint64_t) {
                    res = avail_size == 0;
                    return 0;
                });
                return res;
            }

            // called when initial limits updated
            void apply_initial_limits(const InitialLimits& local, const InitialLimits& peer) {
                control.base.set_initial_limits(local, peer);
                auto apply = [&](const auto& lock, auto& m) {
                    for (auto& [id, s] : m) {
                        s->apply_initial_limits();
                    }
                };
                apply(control.base.open_bidi_lock(), send_bidi);
                apply(control.base.open_uni_lock(), send_uni);
                apply(control.base.accept_bidi_lock(), recv_bidi);
                apply(control.base.accept_uni_lock(), recv_uni);
            }

            std::shared_ptr<SendUniStream<Lock>> open_uni() {
                std::shared_ptr<SendUniStream<Lock>> s;
                control.base.open_uni([&](StreamID id, bool blocked) {
                    if (blocked) {
                        return;
                    }
                    s = internal::make_ptr<SendUniStream<Lock>>(id, borrow_control());
                    send_uni.emplace(id, s);
                    if (open_uni_stream) {
                        open_uni_stream(unisendarg, s);
                    }
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
                    if (open_bidi_stream) {
                        open_bidi_stream(bidisendarg, s);
                    }
                });
                return s;
            }

            void set_open_bidi(std::shared_ptr<void> arg, BidiCB cb) {
                const auto lock = control.base.open_bidi_lock();
                bidisendarg = std::move(arg);
                open_bidi_stream = cb;
            }

            void set_accept_bidi(std::shared_ptr<void> arg, BidiCB cb) {
                const auto lock = control.base.accept_bidi_lock();
                bidirecvarg = std::move(arg);
                accept_bidi_stream = cb;
            }

            void set_open_uni(std::shared_ptr<void> arg, UniOpenCB cb) {
                const auto lock = control.base.open_uni_lock();
                unisendarg = std::move(arg);
                open_uni_stream = cb;
            }

            // cb is  (std::shared_ptr<void> &arg, std::shared_ptr<utils::fnet::quic::stream::impl::RecvUniStream<Lock>> stream)
            void set_accept_uni(std::shared_ptr<void> arg, UniAcceptCB cb) {
                const auto lock = control.base.accept_uni_lock();
                unirecvarg = std::move(arg);
                accept_uni_stream = cb;
            }

            Direction local_dir() const {
                return control.base.state.local_dir();
            }

            Direction peer_dir() const {
                return control.base.state.peer_dir();
            }

            std::shared_ptr<BidiStream<Lock>> find_local_bidi(StreamID id) {
                const auto locked = control.base.open_bidi_lock();
                if (auto it = send_bidi.find(id); it != send_bidi.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<BidiStream<Lock>> find_remote_bidi(StreamID id) {
                const auto locked = control.base.accept_bidi_lock();
                if (auto it = recv_bidi.find(id); it != recv_bidi.end()) {
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

            std::shared_ptr<RecvUniStream<Lock>> find_recv_uni(StreamID id) {
                const auto locked = control.base.accept_uni_lock();
                if (auto it = recv_uni.find(id); it != recv_uni.end()) {
                    return it->second;
                }
                return nullptr;
            }

           private:
            IOResult remove_impl(StreamID id, auto&& locked, auto& streams) {
                auto found = streams.find(id);
                if (found == streams.end()) {
                    return IOResult::no_data;  // already deleted
                }
                if (!found->second->is_removable()) {
                    return IOResult::not_in_io_state;  // should not delete yet
                }
                if (!streams.erase(id)) {
                    return IOResult::fatal;  // BUG!!
                }
                return IOResult::ok;
            }

           public:
            // remove removes stream from manager
            // return values are:
            // ok - successfuly removed
            // fatal - bug
            // invalid_data - invalid stream id
            // no_data - already deleted
            // not_in_io_state - stream is not closed yet
            IOResult remove(StreamID id) {
                if (id.dir() == local_dir()) {
                    if (id.type() == StreamType::bidi) {
                        return remove_impl(id, control.base.open_bidi_lock(), send_bidi);
                    }
                    else if (id.type() == StreamType::uni) {
                        return remove_impl(id, control.base.open_uni_lock(), send_uni);
                    }
                }
                else if (id.dir() == peer_dir()) {
                    if (id.type() == StreamType::bidi) {
                        return remove_impl(id, control.base.accept_bidi_lock(), recv_bidi);
                    }
                    else if (id.type() == StreamType::uni) {
                        return remove_impl(id, control.base.accept_uni_lock(), recv_uni);
                    }
                }
                return IOResult::invalid_data;
            }

           private:
            // return (is_created_by_local,is_bidi,err)
            std::tuple<bool, bool, error::Error> check_creation(FrameType type, StreamID id) {
                auto [by_local, err] = control.base.check_recv_frame(type, id);
                if (err || by_local) {
                    return {true, id.type() == StreamType::bidi, err};
                }
                error::Error oerr;
                control.base.accept(id, [&](StreamID id, bool by_high, error::Error err) {
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
                        if (accept_bidi_stream) {
                            accept_bidi_stream(bidirecvarg, std::move(s));
                        }
                    }
                    else {
                        auto s = internal::make_ptr<RecvUniStream<Lock>>(id, borrow_control());
                        auto [_, ok] = recv_uni.try_emplace(id, s);
                        if (!ok) {
                            oerr = error::Error("unexpected creation failure on uni stream!");
                            return;
                        }
                        if (accept_uni_stream) {
                            accept_uni_stream(unirecvarg, std::move(s));
                        }
                    }
                });
                return {false, id.type() == StreamType::bidi, oerr};
            }

            error::Error recv_stream_related(const frame::Frame& frame, StreamID id) {
                auto [local, bidi, err] = check_creation(frame.type.type_detail(), id);
                if (err) {
                    return err;
                }
                auto maybe_remove = [&](auto&& s) -> error::Error {
                    if (remove_auto && s->is_removable()) {
                        if (auto fat = remove(id); fat == IOResult::fatal) {
                            return error::Error("unexpected error on removing stream");
                        }
                    }
                    return error::none;
                };
                if (local) {
                    if (bidi) {
                        auto bs = find_local_bidi(id);
                        if (!bs) {
                            // already deleted. ignore frame
                            return error::none;
                        }
                        if (auto err = bs->recv(frame)) {
                            return err;
                        }
                        return maybe_remove(bs);
                    }
                    else {
                        auto us = find_send_uni(id);
                        if (!us) {
                            // already deleted. ignore frame
                            return error::none;
                        }
                        us->recv(frame);
                        return maybe_remove(us);
                    }
                }
                else {
                    if (bidi) {
                        auto bs = find_remote_bidi(id);
                        if (!bs) {
                            // already deleted. ignore frame
                            return error::none;
                        }
                        if (auto err = bs->recv(frame)) {
                            return err;
                        }
                        return maybe_remove(bs);
                    }
                    else {
                        auto us = find_recv_uni(id);
                        if (!us) {
                            // already deleted. ignore frame
                            return error::none;
                        }
                        if (auto err = us->recv(frame)) {
                            return err;
                        }
                        return maybe_remove(us);
                    }
                }
            }

           public:
            // returns (is_handled,err)
            std::pair<bool, error::Error> recv(const frame::Frame& frame) {
                auto [id, is_related] = frame::get_streamID(frame);
                if (is_related) {
                    return {true, recv_stream_related(frame, id)};
                }
                if (is_ConnRelated(frame.type.type_detail())) {
                    return {true, control.recv(frame)};
                }
                return {false, error::none};
            }

           private:
            // not concern about priority or evenity
            // should implement send_schedule for your application purpose
            IOResult default_send_schedule(frame::fwriter& fw, auto&& observer_vec) {
                IOResult r = IOResult::ok;
                auto maybe_remove = [&](auto& it, auto& m) {
                    if (remove_auto && it->second->is_removable()) {
                        it = m.erase(it);
                        return true;
                    }
                    return false;
                };
                auto do_send = [&](const auto& lock, auto& m) {
                    for (auto it = m.begin(); it != m.end();) {
                        auto& s = it->second;
                        auto [res, fin] = s->send(fw, observer_vec);
                        if (res == IOResult::fatal || res == IOResult::invalid_data) {
                            r = res;
                            return true;
                        }
                        if (maybe_remove(it, m)) {
                            continue;
                        }
                        it++;
                    }
                    return false;
                };
                auto do_send_r = [&](const auto& lock, auto& m) {
                    for (auto it = m.begin(); it != m.end();) {
                        auto& s = it->second;
                        auto res = s->send(fw, observer_vec);
                        if (res == IOResult::fatal || res == IOResult::invalid_data) {
                            r = res;
                            return true;
                        }
                        if (maybe_remove(it, m)) {
                            continue;
                        }
                        it++;
                    }
                    return false;
                };
                (void)(do_send(control.base.open_bidi_lock(), send_bidi) ||
                       do_send(control.base.open_uni_lock(), send_uni) ||
                       do_send(control.base.accept_bidi_lock(), recv_bidi) ||
                       do_send_r(control.base.accept_uni_lock(), recv_uni));
                return r;
            }

           public:
            IOResult send(frame::fwriter& fw, auto&& observer_vec) {
                if (auto res = control.send(fw, observer_vec); res == IOResult::fatal) {
                    return res;
                }
                if (send_schedule) {
                    return send_schedule(SendeSchedArg{
                                             .unisendarg = unisendarg,
                                             .bidisendarg = bidisendarg,
                                             .unirecvarg = unirecvarg,
                                             .bidirecvarg = bidirecvarg,

                                         },
                                         fw, observer_vec);
                }
                return default_send_schedule(fw, observer_vec);
            }

            // update is std::pair<std::uint64_t/*new_limit*/,bool/*should_update*/>(Limiter)
            bool update_max_uni_streams(auto&& update) {
                return control.update_max_uni_streams(update);
            }
            // update is std::pair<std::uint64_t/*new_limit*/,bool/*should_update*/>(Limiter)
            bool update_max_bidi_streams(auto&& update) {
                return control.update_max_uni_streams(update);
            }

            // update is std::pair<std::uint64_t/*new_limit*/,bool/*should_update*/>(Limiter)
            bool update_max_data(auto&& update) {
                return control.update_max_data(update);
            }
        };

        template <class Lock>
        std::shared_ptr<Conn<Lock>> make_conn(Direction self) {
            return internal::make_ptr<Conn<Lock>>(self);
        }
    }  // namespace fnet::quic::stream::impl
}  // namespace utils

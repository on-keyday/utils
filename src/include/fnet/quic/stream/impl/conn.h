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

        template <class TypeConfigs>
        struct SendeSchedArg {
            using UniOpenArg = typename TypeConfigs::callback_arg::open_uni;
            using UniAcceptArg = typename TypeConfigs::callback_arg::accept_uni;
            using BidiOpenArg = typename TypeConfigs::callback_arg::open_bidi;
            using BidiAcceptArg = typename TypeConfigs::callback_arg::accept_bidi;

            UniOpenArg uniopenarg;
            UniAcceptArg bidiopenarg;
            BidiOpenArg uniacceptarg;
            BidiAcceptArg bidiacceptarg;
        };

        template <class TypeConfigs>
        struct Conn : std::enable_shared_from_this<Conn<TypeConfigs>> {
            using UniOpenArg = typename TypeConfigs::callback_arg::open_uni;
            using UniAcceptArg = typename TypeConfigs::callback_arg::accept_uni;
            using BidiOpenArg = typename TypeConfigs::callback_arg::open_bidi;
            using BidiAcceptArg = typename TypeConfigs::callback_arg::accept_bidi;

            using UniOpenCB = void (*)(UniOpenArg& arg, std::shared_ptr<SendUniStream<TypeConfigs>> stream);
            using UniAcceptCB = void (*)(UniAcceptArg& arg, std::shared_ptr<RecvUniStream<TypeConfigs>> stream);
            using BidiOpenCB = void (*)(UniOpenArg& arg, std::shared_ptr<BidiStream<TypeConfigs>> stream);
            using BidiAcceptCB = void (*)(UniAcceptArg& arg, std::shared_ptr<BidiStream<TypeConfigs>> stream);
            using SendCB = IOResult (*)(SendeSchedArg<TypeConfigs>, frame::fwriter& w, ack::ACKCollector observer_vec);

           private:
            template <class K, class V>
            using stream_map = typename TypeConfigs::template stream_map<K, V>;

            ConnFlowControl<TypeConfigs> control;
            stream_map<StreamID, std::shared_ptr<SendUniStream<TypeConfigs>>> send_uni;
            stream_map<StreamID, std::shared_ptr<BidiStream<TypeConfigs>>> send_bidi;
            stream_map<StreamID, std::shared_ptr<RecvUniStream<TypeConfigs>>> recv_uni;
            stream_map<StreamID, std::shared_ptr<BidiStream<TypeConfigs>>> recv_bidi;
            std::atomic_bool remove_auto = false;

            UniOpenArg uniopenarg;
            BidiOpenArg bidiopenarg;
            UniOpenCB open_uni_stream = nullptr;
            BidiOpenCB open_bidi_stream = nullptr;

            UniAcceptArg uniacceptarg;
            BidiAcceptArg bidiacceptarg;
            UniAcceptCB accept_uni_stream = nullptr;
            BidiOpenCB accept_bidi_stream = nullptr;

            SendCB send_schedule = nullptr;

            auto borrow_control() {
                return std::shared_ptr<ConnFlowControl<TypeConfigs>>(
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
            void apply_local_initial_limits(const InitialLimits& local) {
                control.base.set_local_initial_limits(local);
                auto apply = [&](const auto& lock, auto& m) {
                    for (auto& [id, s] : m) {
                        s->apply_local_initial_limits();
                    }
                };
                apply(control.base.open_bidi_lock(), send_bidi);
                apply(control.base.accept_bidi_lock(), recv_bidi);
                apply(control.base.accept_uni_lock(), recv_uni);
            }

            // called when initial limits updated
            void apply_peer_initial_limits(const InitialLimits& peer) {
                control.base.set_peer_initial_limits(peer);
                auto apply = [&](const auto& lock, auto& m) {
                    for (auto& [id, s] : m) {
                        s->apply_peer_initial_limits();
                    }
                };
                apply(control.base.open_bidi_lock(), send_bidi);
                apply(control.base.open_uni_lock(), send_uni);
                apply(control.base.accept_bidi_lock(), recv_bidi);
            }

            std::shared_ptr<SendUniStream<TypeConfigs>> open_uni() {
                std::shared_ptr<SendUniStream<TypeConfigs>> s;
                control.base.open_uni([&](StreamID id, bool blocked) {
                    if (blocked) {
                        return;
                    }
                    s = internal::make_ptr<SendUniStream<TypeConfigs>>(id, borrow_control());
                    send_uni.emplace(id, s);
                    if (open_uni_stream) {
                        open_uni_stream(uniopenarg, s);
                    }
                });
                return s;
            }

            std::shared_ptr<BidiStream<TypeConfigs>> open_bidi() {
                std::shared_ptr<BidiStream<TypeConfigs>> s;
                control.base.open_bidi([&](StreamID id, bool blocked) {
                    if (blocked) {
                        return;
                    }
                    s = std::allocate_shared<BidiStream<TypeConfigs>>(
                        glheap_allocator<BidiStream<TypeConfigs>>{},
                        id, borrow_control());
                    send_bidi.emplace(id, s);
                    if (open_bidi_stream) {
                        open_bidi_stream(bidiopenarg, s);
                    }
                });
                return s;
            }

            void set_open_bidi(BidiOpenArg arg, BidiOpenCB cb) {
                const auto lock = control.base.open_bidi_lock();
                bidiopenarg = std::move(arg);
                open_bidi_stream = cb;
            }

            void set_accept_bidi(BidiAcceptArg arg, BidiOpenCB cb) {
                const auto lock = control.base.accept_bidi_lock();
                bidiacceptarg = std::move(arg);
                accept_bidi_stream = cb;
            }

            void set_open_uni(UniOpenArg arg, UniOpenCB cb) {
                const auto lock = control.base.open_uni_lock();
                uniopenarg = std::move(arg);
                open_uni_stream = cb;
            }

            void set_accept_uni(UniAcceptArg arg, UniAcceptCB cb) {
                const auto lock = control.base.accept_uni_lock();
                uniacceptarg = std::move(arg);
                accept_uni_stream = cb;
            }

            Direction local_dir() const {
                return control.base.state.local_dir();
            }

            Direction peer_dir() const {
                return control.base.state.peer_dir();
            }

            std::shared_ptr<BidiStream<TypeConfigs>> find_local_bidi(StreamID id) {
                const auto locked = control.base.open_bidi_lock();
                if (auto it = send_bidi.find(id); it != send_bidi.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<BidiStream<TypeConfigs>> find_remote_bidi(StreamID id) {
                const auto locked = control.base.accept_bidi_lock();
                if (auto it = recv_bidi.find(id); it != recv_bidi.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<SendUniStream<TypeConfigs>> find_send_uni(StreamID id) {
                const auto locked = control.base.open_uni_lock();
                if (auto it = send_uni.find(id); it != send_uni.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<RecvUniStream<TypeConfigs>> find_recv_uni(StreamID id) {
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
                        auto s = internal::make_ptr<BidiStream<TypeConfigs>>(id, borrow_control());
                        auto [_, ok] = recv_bidi.try_emplace(id, s);
                        if (!ok) {
                            oerr = error::Error("unexpected cration failure on bidi stream!");
                            return;
                        }
                        if (accept_bidi_stream) {
                            accept_bidi_stream(bidiacceptarg, std::move(s));
                        }
                    }
                    else {
                        auto s = internal::make_ptr<RecvUniStream<TypeConfigs>>(id, borrow_control());
                        auto [_, ok] = recv_uni.try_emplace(id, s);
                        if (!ok) {
                            oerr = error::Error("unexpected creation failure on uni stream!");
                            return;
                        }
                        if (accept_uni_stream) {
                            accept_uni_stream(uniacceptarg, std::move(s));
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
                    return send_schedule(SendeSchedArg<TypeConfigs>{
                                             .uniopenarg = uniopenarg,
                                             .bidiopenarg = bidiopenarg,
                                             .uniacceptarg = uniacceptarg,
                                             .bidiacceptarg = bidiacceptarg,

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

        template <class TypeConfigs>
        std::shared_ptr<Conn<TypeConfigs>> make_conn(Direction self) {
            return internal::make_ptr<Conn<TypeConfigs>>(self);
        }
    }  // namespace fnet::quic::stream::impl
}  // namespace utils

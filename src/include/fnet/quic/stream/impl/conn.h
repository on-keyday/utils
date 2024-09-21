/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include "conn_handler_interface.h"
#include "../../type_macro.h"

namespace futils {
    namespace fnet::quic::stream::impl {

        namespace internal {
            template <class T, class... Args>
            std::shared_ptr<T> make_ptr(Args&&... arg) {
                return std::allocate_shared<T>(glheap_allocator<T>{}, std::forward<Args>(arg)...);
            }
        }  // namespace internal

        template <class TConfig>
        struct SendSchedArg {
            using UniOpenArg = typename TConfig::conn_cb_arg::open_uni;
            using UniAcceptArg = typename TConfig::conn_cb_arg::accept_uni;
            using BidiOpenArg = typename TConfig::conn_cb_arg::open_bidi;
            using BidiAcceptArg = typename TConfig::conn_cb_arg::accept_bidi;

            UniOpenArg uniopenarg;
            UniAcceptArg bidiopenarg;
            BidiOpenArg uniacceptarg;
            BidiAcceptArg bidiacceptarg;
        };

        template <class TConfig>
        struct Conn : std::enable_shared_from_this<Conn<TConfig>> {
            QUIC_ctx_type(ConnHandler, TConfig, template conn_handler<TConfig>);

           private:
            QUIC_ctx_map_type(stream_map, TConfig, stream_map);

            ConnFlowControl<TConfig> control;
            stream_map<StreamID, std::shared_ptr<SendUniStream<TConfig>>> local_uni;
            stream_map<StreamID, std::shared_ptr<BidiStream<TConfig>>> local_bidi;
            stream_map<StreamID, std::shared_ptr<RecvUniStream<TConfig>>> remote_uni;
            stream_map<StreamID, std::shared_ptr<BidiStream<TConfig>>> remote_bidi;
            std::atomic_bool remove_auto = false;

            // ConnHandler handler;
            ConnHandlerInterface<ConnHandler> handler;

            auto borrow_control() {
                return std::shared_ptr<ConnFlowControl<TConfig>>(
                    this->shared_from_this(), &control);
            }

           public:
            Conn(Origin self, std::weak_ptr<void> conn_ctx) {
                control.base.state.set_dir(self);
                control.conn_ctx = std::move(conn_ctx);
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
                apply(control.base.open_bidi_lock(), local_bidi);
                apply(control.base.accept_bidi_lock(), remote_bidi);
                apply(control.base.accept_uni_lock(), remote_uni);
            }

            // called when initial limits updated
            void apply_peer_initial_limits(const InitialLimits& peer) {
                control.base.set_peer_initial_limits(peer);
                auto apply = [&](const auto& lock, auto& m) {
                    for (auto& [id, s] : m) {
                        s->apply_peer_initial_limits();
                    }
                };
                apply(control.base.open_bidi_lock(), local_bidi);
                apply(control.base.open_uni_lock(), local_uni);
                apply(control.base.accept_bidi_lock(), remote_bidi);
            }

            std::shared_ptr<SendUniStream<TConfig>> open_uni() {
                std::shared_ptr<SendUniStream<TConfig>> s;
                control.base.open_uni([&](StreamID id, bool blocked) {
                    if (blocked) {
                        return;
                    }
                    s = internal::make_ptr<SendUniStream<TConfig>>(id, borrow_control());
                    local_uni.emplace(id, s);
                    handler.uni_open(s);
                });
                return s;
            }

            std::shared_ptr<BidiStream<TConfig>> open_bidi() {
                std::shared_ptr<BidiStream<TConfig>> s;
                control.base.open_bidi([&](StreamID id, bool blocked) {
                    if (blocked) {
                        return;
                    }
                    s = std::allocate_shared<BidiStream<TConfig>>(
                        glheap_allocator<BidiStream<TConfig>>{},
                        id, borrow_control());
                    local_bidi.emplace(id, s);
                    handler.bidi_open(s);
                });
                return s;
            }

            // thread unsafe call
            // call this before share this with multi thread
            auto get_conn_handler() {
                return handler.impl_ptr();
            }

            Origin local_origin() const {
                return control.base.state.local_origin();
            }

            Origin peer_origin() const {
                return control.base.state.peer_origin();
            }

            size_t local_bidi_avail() {
                const auto locked = control.base.open_bidi_lock();
                return local_bidi.size();
            }

            size_t remote_bidi_avail() {
                const auto locked = control.base.accept_bidi_lock();
                return remote_bidi.size();
            }

            size_t local_uni_avail() {
                const auto locked = control.base.open_uni_lock();
                return local_uni.size();
            }

            size_t remote_uni_avail() {
                const auto locked = control.base.accept_uni_lock();
                return remote_uni.size();
            }

            std::shared_ptr<BidiStream<TConfig>> find_local_bidi(StreamID id) {
                const auto locked = control.base.open_bidi_lock();
                if (auto it = local_bidi.find(id); it != local_bidi.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<BidiStream<TConfig>> find_remote_bidi(StreamID id) {
                const auto locked = control.base.accept_bidi_lock();
                if (auto it = remote_bidi.find(id); it != remote_bidi.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<BidiStream<TConfig>> find_bidi(StreamID id) {
                if (id.origin() == local_origin()) {
                    return find_local_bidi(id);
                }
                return find_remote_bidi(id);
            }

            std::shared_ptr<SendUniStream<TConfig>> find_send_uni(StreamID id) {
                const auto locked = control.base.open_uni_lock();
                if (auto it = local_uni.find(id); it != local_uni.end()) {
                    return it->second;
                }
                return nullptr;
            }

            std::shared_ptr<RecvUniStream<TConfig>> find_recv_uni(StreamID id) {
                const auto locked = control.base.accept_uni_lock();
                if (auto it = remote_uni.find(id); it != remote_uni.end()) {
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
            // ok - successfully removed
            // fatal - bug
            // invalid_data - invalid stream id
            // no_data - already deleted or not created yet
            // not_in_io_state - stream is not closed yet
            IOResult remove(StreamID id) {
                if (id.origin() == local_origin()) {
                    if (id.type() == StreamType::bidi) {
                        return remove_impl(id, control.base.open_bidi_lock(), local_bidi);
                    }
                    else if (id.type() == StreamType::uni) {
                        return remove_impl(id, control.base.open_uni_lock(), local_uni);
                    }
                }
                else if (id.origin() == peer_origin()) {
                    if (id.type() == StreamType::bidi) {
                        return remove_impl(id, control.base.accept_bidi_lock(), remote_bidi);
                    }
                    else if (id.type() == StreamType::uni) {
                        return remove_impl(id, control.base.accept_uni_lock(), remote_uni);
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
                        auto s = internal::make_ptr<BidiStream<TConfig>>(id, borrow_control());
                        auto [_, ok] = remote_bidi.try_emplace(id, s);
                        if (!ok) {
                            oerr = error::Error("unexpected creation failure on bidi stream!", error::Category::lib, error::fnet_quic_implementation_bug);
                            return;
                        }

                        handler.bidi_accept(std::move(s));
                    }
                    else {
                        auto s = internal::make_ptr<RecvUniStream<TConfig>>(id, borrow_control());
                        auto [_, ok] = remote_uni.try_emplace(id, s);
                        if (!ok) {
                            oerr = error::Error("unexpected creation failure on uni stream!", error::Category::lib, error::fnet_quic_implementation_bug);
                            return;
                        }

                        handler.uni_accept(std::move(s));
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
                            return error::Error("unexpected error on removing stream", error::Category::lib, error::fnet_quic_stream_error);
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
                const auto d = helper::defer([&] {
                    handler.recv_callback(this);
                });
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
            IOResult default_send_schedule(frame::fwriter& fw, auto&& observer) {
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
                        auto [res, fin] = s->send(fw, observer);
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
                        auto res = s->send(fw, observer);
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
                (void)(do_send(control.base.open_bidi_lock(), local_bidi) ||
                       do_send(control.base.open_uni_lock(), local_uni) ||
                       do_send(control.base.accept_bidi_lock(), remote_bidi) ||
                       do_send_r(control.base.accept_uni_lock(), remote_uni));
                return r;
            }

           public:
            IOResult send(frame::fwriter& fw, auto&& observer) {
                handler.send_callback(this);
                if (auto res = control.send(fw, observer); res == IOResult::fatal) {
                    return res;
                }
                auto do_default = [&] {
                    return default_send_schedule(fw, observer);
                };
                return handler.send_schedule(fw, observer, do_default);
            }

            void iterate_local_bidi_stream(auto&& cb) {
                const auto locked = control.base.open_bidi_lock();
                for (auto& [id, s] : local_bidi) {
                    cb(s);
                }
            }

            void iterate_remote_bidi_stream(auto&& cb) {
                const auto locked = control.base.accept_bidi_lock();
                for (auto& [id, s] : remote_bidi) {
                    cb(s);
                }
            }

            void iterate_bidi_stream(auto&& cb) {
                iterate_local_bidi_stream(cb);
                iterate_remote_bidi_stream(cb);
            }

            void iterate_send_uni_stream(auto&& cb) {
                const auto locked = control.base.open_uni_lock();
                for (auto& [id, s] : local_uni) {
                    cb(s);
                }
            }

            void iterate_recv_uni_stream(auto&& cb) {
                const auto locked = control.base.accept_uni_lock();
                for (auto& [id, s] : remote_uni) {
                    cb(s);
                }
            }

            // update is std::uint64_t/*new_limit*/(Limiter,std::uint64_t/*initial limit*/)
            // return value less than current limit has no effect to update limit
            bool update_max_uni_streams(auto&& update) {
                return control.update_max_uni_streams(update);
            }

            // update is std::uint64_t/*new_limit*/(Limiter,std::uint64_t/*initial limit*/)
            // return value less than current limit has no effect to update limit
            bool update_max_bidi_streams(auto&& update) {
                return control.update_max_uni_streams(update);
            }

            // update is std::uint64_t/*new_limit*/(Limiter,std::uint64_t/*initial limit*/)
            // return value less than current limit has no effect to update limit
            bool update_max_data(auto&& update) {
                return control.update_max_data(update);
            }
        };

        template <class TConfig>
        std::shared_ptr<Conn<TConfig>> make_conn(Origin self, std::shared_ptr<void> conn_ctx) {
            return internal::make_ptr<Conn<TConfig>>(self, conn_ctx);
        }
    }  // namespace fnet::quic::stream::impl
}  // namespace futils

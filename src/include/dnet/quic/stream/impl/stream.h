/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream - connection implementaion
#pragma once
#include "conn.h"
#include "../stream_base.h"
#include "../../../std/hash_map.h"
#include "../../../std/vector.h"
#include "../../../storage.h"
#include "../../ack/ack_lost_record.h"
#include "../../../../helper/condincr.h"
#include <algorithm>

namespace utils {
    namespace dnet::quic::stream::impl {
        struct ACKLostFragment {
            std::uint64_t offset = 0;
            flex_storage fragment;
            bool fin = false;
            std::shared_ptr<ack::ACKLostRecord> wait;
        };

        template <class Lock>
        struct SendUniStream {
           private:
            SendUniStreamBase<Lock> uni;
            std::weak_ptr<ConnFlowControl<Lock>> conn;
            using Map = slib::hash_map<packetnum::Value, ACKLostFragment>;
            Map fragments;

            flex_storage src;
            std::shared_ptr<ack::ACKLostRecord> reset_wait;

            static bool save_new_fragment(auto& fragment_store, packetnum::Value pn, Fragment frag, auto&& observer_vec) {
                auto [it, ok] = fragment_store.try_emplace(pn, ACKLostFragment{});
                if (!ok) {
                    return false;
                }
                it->second.offset = frag.offset;
                it->second.fin = frag.fin;
                it->second.fragment = frag.fragment;
                it->second.wait = ack::make_ack_wait();
                observer_vec.push_back(it->second.wait);
                return true;
            }

            IOResult send_retransmit(packetnum::Value pn, auto&& observer_vec, frame::fwriter& w) {
                bool removed = false;
                const auto incr = helper::cond_incr(removed);
                Map tmp_storage;
                bool least_one = false;
                for (auto it = fragments.begin(); it != fragments.end(); incr(it)) {
                    if (it->second.wait->is_ack()) {
                        it = fragments.erase(it);  // remove acked range
                        removed = true;
                        continue;
                    }
                    if (it->second.wait->is_lost()) {
                        auto save_retransmit = [&](Fragment sent, Fragment remain, bool has_remain) {
                            if (!save_new_fragment(tmp_storage, pn, sent, observer_vec)) {
                                return false;
                            }
                            if (has_remain) {
                                it->second.fragment = flex_storage(remain.fragment);
                                it->second.offset = remain.offset;
                                it->second.fin = remain.fin;
                                // wait next chance
                            }
                            else {
                                it = fragments.erase(it);  // all is moved to new fragment
                                removed = true;
                            }
                            return true;
                        };
                        auto res = uni.send_retransmit(w, it->second.offset, it->second.fragment, it->second.fin, save_retransmit);
                        if (res == IOResult::fatal || res == IOResult::invalid_data) {
                            return res;  // fatal error
                        }
                        if (res == IOResult::ok) {
                            least_one = true;
                        }
                    }
                }
                fragments.merge(tmp_storage);
                return least_one ? IOResult::ok : IOResult::no_data;
            }

           public:
            SendUniStream(StreamID id, std::shared_ptr<ConnFlowControl<Lock>> c) {
                uni.id = id;
                conn = c;
                uni.set_initial(c->base.state.send_ini_limit, c->base.state.local_dir());
            }

            constexpr StreamID id() const {
                return uni.id;
            }

            SendState detect_ack() {
                const auto locked = uni.lock();
                if (reset_wait) {
                    if (reset_wait->is_ack()) {
                        auto res = uni.reset_done();
                        reset_wait = nullptr;
                        return uni.state.state;
                    }
                }
                bool removed = false;
                const auto incr = helper::cond_incr(removed);
                for (auto it = fragments.begin(); it != fragments.end(); incr(it)) {
                    if (it->second.wait->is_ack()) {
                        it = fragments.erase(it);
                        removed = true;
                    }
                }
                if (uni.state.state == SendState::data_sent && fragments.size() == 0) {
                    uni.all_send_done();
                }
                return uni.state.state;
            }

            IOResult add_data(view::rvec data, bool fin) {
                const auto locked = uni.lock();
                if (uni.data.fin) {
                    return IOResult::cancel;
                }
                src.append(data);
                return uni.update_data(0, src, fin) ? IOResult::ok : IOResult::fatal;
            }

            IOResult send(packetnum::Value pn, frame::fwriter& w, auto&& observer_vec) {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return IOResult::fatal;  // no way to recover
                }
                auto [res, blocked_size] = uni.send(c->base, w, [&](Fragment frag) {
                    return save_new_fragment(fragments, pn, frag, observer_vec);
                });
                if (res == IOResult::not_in_io_state || res == IOResult::no_data) {
                    if (uni.state.state != SendState::data_recved) {
                        return send_retransmit(pn, observer_vec, w);
                    }
                    return res;
                }
                if (res == IOResult::ok || res == IOResult::fatal) {
                    return res;
                }
                if (res == IOResult::block_by_stream) {
                    return send_stream_blocked(w, uni.id, blocked_size);
                }
                if (res == IOResult::block_by_conn) {
                    return send_conn_blocked(w, blocked_size);
                }
                return IOResult::ok;  // may be Result::no_capacity
            }

            IOResult request_reset(std::uint64_t code) {
                const auto locked = uni.lock();
                return uni.request_reset(code);
            }

            IOResult send_reset(frame::fwriter& w, auto&& observer_vec) {
                const auto locked = uni.lock();
                if (reset_wait) {
                    if (reset_wait->is_ack()) {
                        return uni.reset_done();
                    }
                    if (reset_wait->is_lost()) {
                        auto res = uni.send_reset_retransmit(w);
                        if (res == IOResult::ok) {
                            reset_wait->wait();
                            observer_vec.push_back(reset_wait);
                        }
                        return res;
                    }
                    return IOResult::ok;  // nothing to do
                }
                auto r = uni.send_reset_if_stop_required(w);
                if (r != IOResult::ok) {
                    return r;
                }
                reset_wait = ack::make_ack_wait();
                observer_vec.push_back(reset_wait);
                return IOResult::ok;
            }

            void recv_stop_sending(const frame::StopSendingFrame& frame) {
                const auto locked = uni.lock();
                uni.recv_stop_sending(frame);
            }

            void recv_max_stream_data(const frame::MaxStreamDataFrame& frame) {
                const auto locked = uni.lock();
                uni.recv_max_stream_data(frame);
            }

            void set_initial(const InitialLimits& ini) {
                const auto locked = uni.lock();
                uni.set_initial(ini);
            }
        };

        struct RecvData {
            std::uint64_t offset = 0;
            flex_storage data;
            packetnum::Value packet_number;
        };

        template <class Lock>
        struct RecvUniStream {
           private:
            RecvUniStreamBase<Lock> uni;
            std::weak_ptr<ConnFlowControl<Lock>> conn;
            std::shared_ptr<void> arg;
            error::Error (*user_operation)(std::shared_ptr<void>& arg, slib::vector<RecvData>& data) = nullptr;
            slib::vector<RecvData> data;
            std::shared_ptr<ack::ACKLostRecord> max_data;
            std::shared_ptr<ack::ACKLostRecord> stop_sending;

           public:
            RecvUniStream(StreamID id, std::shared_ptr<ConnFlowControl<Lock>> c) {
                uni.id = id;
                conn = c;
                uni.set_initial(c->base.state.recv_ini_limit, c->base.state.local_dir());
            }

            constexpr StreamID id() const {
                return uni.id;
            }

            error::Error recv(packetnum::Value pn, const frame::StreamFrame& frame) {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return error::eof;
                }
                auto [_, err] = uni.recv(c->base, frame, [&](Fragment frag) -> error::Error {
                    RecvData buf;
                    buf.data = frag.fragment;
                    buf.offset = frag.offset;
                    buf.packet_number = pn;
                    data.push_back(std::move(buf));
                    if (user_operation) {
                        return user_operation(arg, data);
                    }
                    return error::none;
                });
                return err;
            }

            error::Error recv_reset(const frame::ResetStreamFrame& frame) {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return error::eof;
                }
                auto [_, err] = uni.recv_reset(c->base, frame);
                return err;
            }

            IOResult send_stop_sending(frame::fwriter& w, auto&& observer_vec) {
                const auto locked = uni.lock();
                if (stop_sending) {
                    if (stop_sending->is_lost()) {
                        if (auto res = uni.send_stop_sending(w); res != IOResult::ok) {
                            return res;
                        }
                        stop_sending->wait();
                        observer_vec.push_back(stop_sending);
                        return IOResult::ok;
                    }
                    if (stop_sending->is_ack()) {
                        return IOResult::ok;
                    }
                }
                if (auto res = uni.send_stop_sending(w); res != IOResult::ok) {
                    return res;
                }
                stop_sending = ack::make_ack_wait();
                observer_vec.push_back(stop_sending);
                return IOResult::ok;
            }

            IOResult update_recv_limit(auto&& decide_new_limit) {
                const auto locked = uni.lock();
                return uni.update_recv_limit(decide_new_limit);
            }

            IOResult send_max_stream_data(frame::fwriter& w, auto&& observer_vec) {
                const auto locked = uni.lock();
                auto res = uni.send_max_stream_data_if_updated(w);
                if (res == IOResult::ok) {
                    // clear old ACKLostRecord
                    max_data = ack::make_ack_wait();
                    observer_vec.push_back(max_data);
                    return res;
                }
                if (max_data) {
                    if (max_data->is_lost()) {
                        res = uni.send_max_stream_data(w);
                        if (res != IOResult::ok) {
                            return res;
                        }
                        max_data->wait();
                        observer_vec.push_back(max_data);
                    }
                    else if (max_data->is_ack()) {
                        max_data = nullptr;
                    }
                }
                return IOResult::ok;
            }

            bool request_stop_sending(std::uint64_t code) {
                const auto lock = uni.lock();
                return uni.request_stop_sending(code);
            }
        };

        template <class Lock>
        struct BidiStream {
            SendUniStream<Lock> send;
            RecvUniStream<Lock> recv;

            BidiStream(StreamID id,
                       std::shared_ptr<ConnFlowControl<Lock>> conn)
                : send(id, conn), recv(id, conn) {}
        };
    }  // namespace dnet::quic::stream::impl
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream - connection implementaion
#pragma once
#include "conn_flow.h"
#include "../stream_base.h"
#include "../../../std/list.h"
#include "../../../std/vector.h"
#include "../../../storage.h"
#include "../../ack/ack_lost_record.h"
#include "../../../../helper/condincr.h"
#include <algorithm>

namespace utils {
    namespace dnet::quic::stream::impl {
        struct SentFragment {
            std::uint64_t offset = 0;
            flex_storage fragment;
            std::shared_ptr<ack::ACKLostRecord> wait;
            bool fin = false;
        };

        template <class Lock>
        struct SendUniStream {
           private:
            using List = slib::list<SentFragment>;

            SendUniStreamBase<Lock> uni;
            std::weak_ptr<ConnFlowControl<Lock>> conn;
            List fragments;
            flex_storage src;
            std::shared_ptr<ack::ACKLostRecord> reset_wait;

            static bool save_new_fragment(auto& fragment_store, Fragment frag, auto&& observer_vec) {
                SentFragment& it = fragment_store.emplace_back(SentFragment{});
                it.offset = frag.offset;
                it.fin = frag.fin;
                it.fragment = frag.fragment;
                it.wait = ack::make_ack_wait();
                observer_vec.push_back(it.wait);
                return true;
            }

            // move state to data_recved if condition satisfied
            void maybe_send_done() {
                if (uni.state.state == SendState::data_sent && fragments.size() == 0) {
                    uni.all_send_done();
                }
            }

            // do STREAM frame retransmission
            IOResult send_retransmit(auto&& observer_vec, frame::fwriter& w) {
                if (!uni.state.can_retransmit()) {
                    return IOResult::not_in_io_state;
                }
                List retransmit_list;
                bool least_one = false;
                for (auto it = fragments.begin(); it != fragments.end();) {
                    if (it->wait->is_ack()) {
                        it = fragments.erase(it);  // remove acked fragments
                        continue;
                    }
                    if (it->wait->is_lost()) {
                        bool removed = false;
                        auto save_retransmit = [&](Fragment sent, Fragment remain, bool has_remain) {
                            if (!save_new_fragment(retransmit_list, sent, observer_vec)) {
                                return false;
                            }
                            if (has_remain) {
                                it->fragment = flex_storage(remain.fragment);
                                it->offset = remain.offset;
                                it->fin = remain.fin;
                                // wait next chance to send
                            }
                            else {
                                it = fragments.erase(it);  // all is moved to new fragment
                                removed = true;
                            }
                            return true;
                        };
                        auto res = uni.send_retransmit_stream(w, it->offset, it->fragment, it->fin, save_retransmit);
                        if (res == IOResult::fatal || res == IOResult::invalid_data) {
                            return res;  // fatal error
                        }
                        if (res == IOResult::ok) {
                            least_one = true;
                        }
                        if (removed) {
                            continue;
                        }
                    }
                    it++;
                }
                fragments.splice(fragments.end(), std::move(retransmit_list));
                maybe_send_done();
                return least_one ? IOResult::ok : IOResult::no_data;
            }

            IOResult send_stream_unlocked(frame::fwriter& w, auto&& observer_vec) {
                auto c = conn.lock();
                if (!c) {
                    return IOResult::fatal;  // no way to recover
                }
                size_t blocked_size;
                // first try retransmition
                IOResult res = send_retransmit(observer_vec, w);
                if (res != IOResult::ok && res != IOResult::no_data &&
                    res != IOResult::not_in_io_state) {
                    return res;
                }
                // send STREAM frame
                std::tie(res, blocked_size) = uni.send_stream(c->base, w, [&](Fragment frag) {
                    return save_new_fragment(fragments, frag, observer_vec);
                });
                if (res == IOResult::block_by_stream) {
                    res = send_stream_blocked(w, uni.id, blocked_size);
                    return res == IOResult::ok ? IOResult::block_by_stream : res;
                }
                if (res == IOResult::block_by_conn) {
                    res = send_conn_blocked(w, blocked_size);
                    return res == IOResult::ok ? IOResult::block_by_conn : res;
                }
                return res;
            }

            IOResult send_reset_unlocked(frame::fwriter& w, auto&& observer_vec) {
                if (reset_wait) {
                    if (reset_wait->is_ack()) {
                        return uni.reset_done();
                    }
                    if (reset_wait->is_lost()) {
                        auto res = uni.send_retransmit_reset(w);
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

           public:
            SendUniStream(StreamID id, std::shared_ptr<ConnFlowControl<Lock>> c) {
                uni.id = id;
                conn = c;
                uni.set_initial(c->base.state.send_ini_limit, c->base.state.local_dir());
            }

            // called when initial limits updated
            void apply_initial_limits() {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return;  // no meaning
                }
                uni.set_initial(c->base.state.send_ini_limit, c->base.state.local_dir());
            }

            // each methods
            // for test or customized runtime call

            // do ack and lost detection
            // IOResult::ok - to be active
            // IOResult::no_data - not to be active
            // IOResult::not_in_io_state - finished
            IOResult detect_ack_lost() {
                const auto lock = uni.lock();
                if (reset_wait) {
                    if (reset_wait->is_ack()) {
                        uni.reset_done();
                        return IOResult::not_in_io_state;
                    }
                    if (reset_wait->is_lost()) {
                        return IOResult::ok;
                    }
                    return IOResult::no_data;
                }
                bool should_be_active = false;
                for (auto it = fragments.begin(); it != fragments.end();) {
                    if (it->wait->is_ack()) {
                        it = fragments.erase(it);
                        continue;
                    }
                    if (it->wait->is_lost()) {
                        should_be_active = true;
                    }
                    it++;
                }
                maybe_send_done();
                if (uni.state.is_terminal_state()) {
                    return IOResult::not_in_io_state;
                }
                return should_be_active ? IOResult::ok : IOResult::no_data;
            }

            IOResult send_stream(frame::fwriter& w, auto&& observer_vec) {
                const auto lock = uni.lock();
                return send_stream_unlocked(w, observer_vec);
            }

            IOResult send_reset(frame::fwriter& w, auto&& observer_vec) {
                const auto lock = uni.lock();
                return send_reset_unlocked(w, observer_vec);
            }

            void recv_stop_sending(const frame::StopSendingFrame& frame) {
                const auto locked = uni.lock();
                uni.recv_stop_sending(frame);
            }

            void recv_max_stream_data(const frame::MaxStreamDataFrame& frame) {
                const auto locked = uni.lock();
                uni.recv_max_stream_data(frame);
            }

            // general handling
            // called by QUIC runtime system

            // returns (result,finished)
            std::pair<IOResult, bool> send(frame::fwriter& w, auto&& observer_vec) {
                const auto lock = uni.lock();
                if (uni.state.is_terminal_state()) {
                    return {IOResult::ok, true};
                }
                // fire reset if reset sent
                auto res = send_reset_unlocked(w, observer_vec);
                if (res != IOResult::not_in_io_state) {
                    return {res, uni.state.is_terminal_state()};
                }
                res = send_stream_unlocked(w, observer_vec);
                return {res, uni.state.is_terminal_state()};
            }

            bool recv(const frame::Frame& frame) {
                if (frame.type.type_detail() == FrameType::STOP_SENDING) {
                    recv_stop_sending(static_cast<const frame::StopSendingFrame&>(frame));
                    return true;
                }
                if (frame.type.type_detail() == FrameType::MAX_STREAM_DATA) {
                    recv_max_stream_data(static_cast<const frame::MaxStreamDataFrame&>(frame));
                    return true;
                }
                return false;
            }

            // for user operation
            // use subset of these for user class

            constexpr StreamID id() const {
                return uni.id;
            }

            // add send data
            IOResult add_data(view::rvec data, bool fin) {
                const auto locked = uni.lock();
                if (uni.data.fin) {
                    return IOResult::cancel;
                }
                src.append(data);
                return uni.update_data(0, src, fin) ? IOResult::ok : IOResult::fatal;  // must success
            }

            // request cancelation
            IOResult request_reset(std::uint64_t code) {
                const auto locked = uni.lock();
                return uni.request_reset(code);
            }
        };

        template <class Lock>
        struct RecvUniStream {
            // returns (all_recved,err)
            using SaveFragment = std::pair<bool, error::Error> (*)(std::shared_ptr<void>& arg, Fragment frag);

           private:
            RecvUniStreamBase<Lock> uni;
            std::weak_ptr<ConnFlowControl<Lock>> conn;
            std::shared_ptr<void> arg;
            SaveFragment save_fragment = nullptr;
            std::shared_ptr<ack::ACKLostRecord> max_data_wait;
            std::shared_ptr<ack::ACKLostRecord> stop_sending_wait;

            IOResult send_stop_sending_unlocked(frame::fwriter& w, auto&& observer_vec) {
                if (stop_sending_wait) {
                    if (stop_sending_wait->is_lost()) {
                        if (auto res = uni.send_stop_sending(w); res != IOResult::ok) {
                            return res;
                        }
                        stop_sending_wait->wait();
                        observer_vec.push_back(stop_sending_wait);
                        return IOResult::ok;
                    }
                    if (stop_sending_wait->is_ack()) {
                        return IOResult::ok;
                    }
                }
                // try to send STOP_SENDING
                // returns IOResult::not_in_io_state if not in state
                if (auto res = uni.send_stop_sending(w); res != IOResult::ok) {
                    return res;
                }
                stop_sending_wait = ack::make_ack_wait();
                observer_vec.push_back(stop_sending_wait);
                return IOResult::ok;
            }

            IOResult send_max_stream_data_unlocked(frame::fwriter& w, auto&& observer_vec) {
                auto res = uni.send_max_stream_data_if_updated(w);
                if (res == IOResult::ok) {
                    // clear old ACKLostRecord
                    max_data_wait = ack::make_ack_wait();
                    observer_vec.push_back(max_data_wait);
                    return res;
                }
                if (res == IOResult::fatal) {
                    return res;
                }
                if (max_data_wait) {
                    if (max_data_wait->is_lost()) {
                        // retransmit
                        res = uni.send_max_stream_data(w);
                        if (res != IOResult::ok) {
                            if (res == IOResult::not_in_io_state) {
                                // state is not RecvState::Recv
                                max_data_wait = nullptr;
                            }
                            return res;
                        }
                        max_data_wait->wait();
                        observer_vec.push_back(max_data_wait);
                    }
                    else if (max_data_wait->is_ack()) {
                        max_data_wait = nullptr;
                    }
                }
                return IOResult::ok;
            }

           public:
            RecvUniStream(StreamID id, std::shared_ptr<ConnFlowControl<Lock>> c) {
                uni.id = id;
                conn = c;
                uni.set_initial(c->base.state.recv_ini_limit, c->base.state.local_dir());
            }

            void apply_initial_limits() {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return;  // no meaning
                }
                uni.set_initial(c->base.state.recv_ini_limit, c->base.state.local_dir());
            }

            // each methods
            // for test or customized runtime call

            error::Error recv_stream(const frame::StreamFrame& frame) {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return error::eof;  // no way to handle
                }
                auto [_, err] = uni.recv_stream(c->base, frame, [&](Fragment frag) -> error::Error {
                    if (save_fragment) {
                        auto [all_done, err] = save_fragment(arg, frag);
                        if (err) {
                            return err;
                        }
                        if (all_done) {
                            // if all_done returns in illegal state
                            // this function doesn't change state to RecvState::recved
                            uni.state.recved();
                        }
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
                return send_stop_sending_unlocked(w, observer_vec);
            }

            IOResult send_max_stream_data(frame::fwriter& w, auto&& observer_vec) {
                const auto locked = uni.lock();
                return send_stop_sending_unlocked(w, observer_vec);
            }

            // general handling
            // called by QUIC runtime system

            error::Error recv(const frame::Frame& frame) {
                if (is_STREAM(frame.type.type_detail())) {
                    return recv_stream(static_cast<const frame::StreamFrame&>(frame));
                }
                if (frame.type.type_detail() == FrameType::RESET_STREAM) {
                    return recv_reset(static_cast<const frame::ResetStreamFrame&>(frame));
                }
                if (frame.type.type_detail() == FrameType::STREAM_DATA_BLOCKED) {
                    return error::none;
                }
                return error::Error("unexpected frame type");
            }

            IOResult send(frame::fwriter& w, auto&& observer_vec) {
                const auto locked = uni.lock();
                auto res = send_stop_sending_unlocked(w, observer_vec);
                if (res == IOResult::fatal) {
                    return res;
                }
                return send_max_stream_data_unlocked(w, observer_vec);
            }

            // for user operation
            // use subset of these for user class

            constexpr StreamID id() const {
                return uni.id;
            }

            // update recv limit for
            IOResult update_recv_limit(auto&& decide_new_limit) {
                const auto locked = uni.lock();
                return uni.update_recv_limit(decide_new_limit);
            }

            // request stop sending
            bool request_stop_sending(std::uint64_t code) {
                const auto locked = uni.lock();
                return uni.request_stop_sending(code);
            }

            // set receiver user callback
            bool set_receiver(SaveFragment save, std::shared_ptr<void> arg) {
                const auto locked = uni.lock();
                if (uni.state.state == RecvState::pre_recv) {
                    save_fragment = save;
                    this->arg = std::move(arg);
                    return true;
                }
                return false;
            }

            // notify application user read all data
            bool user_read_all() {
                const auto locked = uni.lock();
                return uni.state.data_read();
            }

            // notify application user read reset
            bool user_read_reset() {
                const auto locked = uni.lock();
                return uni.state.reset_read();
            }
        };

        template <class Lock>
        struct BidiStream {
            SendUniStream<Lock> sender;
            RecvUniStream<Lock> receiver;

            // general handling
            // called by QUIC runtime system

            // returns (result,finished)
            std::pair<IOResult, bool> send(frame::fwriter& w, auto&& observer_vec) {
                auto res = receiver.send(w, observer_vec);
                if (res == IOResult::fatal || res == IOResult::invalid_data) {
                    return {res, false};
                }
                return sender.send(w, observer_vec);
            }

            // do recv for each sender or receiver
            error::Error recv(const frame::Frame& frame) {
                if (sender.recv(frame)) {
                    return error::none;
                }
                return receiver.recv(frame);
            }

            void apply_initial_limits() {
                sender.apply_initial_limits();
                receiver.apply_initial_limits();
            }

            BidiStream(StreamID id,
                       std::shared_ptr<ConnFlowControl<Lock>> conn)
                : sender(id, conn), receiver(id, conn) {}
        };
    }  // namespace dnet::quic::stream::impl
}  // namespace utils

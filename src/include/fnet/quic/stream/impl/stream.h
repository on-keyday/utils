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
#include "../../resend/fragments.h"

namespace utils {
    namespace fnet::quic::stream::impl {

        template <class Lock>
        struct SendUniStream {
           private:
            SendUniStreamBase<Lock> uni;
            std::weak_ptr<ConnFlowControl<Lock>> conn;

            flex_storage src;
            std::shared_ptr<ack::ACKLostRecord> reset_wait;
            resend::Retransmiter<resend::StreamFragment> frags;

            // move state to data_recved if condition satisfied
            void maybe_send_done() {
                if (uni.state.is_data_sent() && frags.remain() == 0) {
                    uni.all_send_done();
                }
            }

            // do STREAM frame retransmission
            IOResult send_retransmit(auto&& observer_vec, frame::fwriter& w) {
                if (!uni.state.can_retransmit()) {
                    return IOResult::not_in_io_state;
                }
                auto res = frags.retransmit(observer_vec, [&](resend::StreamFragment& frag, auto&& save_new) {
                    bool should_remove = false;
                    auto save_retransmit = [&](Fragment sent, Fragment remain, bool has_remain) {
                        save_new(resend::StreamFragment{
                            .offset = sent.offset,
                            .data = sent.fragment,
                            .fin = sent.fin,
                        });
                        if (has_remain) {
                            frag.data = flex_storage(remain.fragment);
                            frag.offset = remain.offset;
                            frag.fin = remain.fin;
                            // wait next chance to send
                        }
                        else {
                            should_remove = true;
                        }
                        return true;
                    };
                    auto res = uni.send_retransmit_stream(w, frag.offset, frag.data, frag.fin, save_retransmit);
                    if (res == IOResult::fatal || res == IOResult::invalid_data) {
                        return res;  // fatal error
                    }
                    if (should_remove) {
                        return IOResult::ok;  // remove fragment
                    }
                    return IOResult::not_in_io_state;  // don't delete
                });
                if (res == IOResult::ok) {
                    maybe_send_done();
                }
                return res;
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
                    frags.sent(observer_vec, resend::StreamFragment{
                                                 .offset = frag.offset,
                                                 .data = frag.fragment,
                                                 .fin = frag.fin,
                                             });
                    return true;
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
                        ack::put_ack_wait(std::move(reset_wait));
                        return uni.reset_done();
                    }
                    if (reset_wait->is_lost()) {
                        auto res = uni.send_retransmit_reset(w);
                        if (res == IOResult::ok) {
                            reset_wait = ack::make_ack_wait();
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
                bool should_be_active = frags.detect_ack_and_lost() != 0;
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

            bool recv_max_stream_data(const frame::MaxStreamDataFrame& frame) {
                const auto locked = uni.lock();
                return uni.recv_max_stream_data(frame);
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

            constexpr std::uint64_t get_error_code() const {
                const auto locked = uni.lock();
                return uni.state.get_error_code();
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

            // check whether provided data completely transmited
            bool data_transmited() {
                const auto locked = uni.lock();
                return uni.data.remain() == 0 && fragments.size() == 0;
            }

            // check whetehr stream closed
            bool is_closed() {
                const auto locked = uni.lock();
                return conn.expired() || uni.state.is_terminal_state();
            }

            // report whether stream is removable from conn
            // called by connections remove methods
            bool is_removable() {
                return is_closed();
            }
        };

        // returns (all_recved,err)
        using SaveFragmentCallback = std::pair<bool, error::Error> (*)(std::shared_ptr<void>& arg, StreamID id, FrameType type, Fragment frag, std::uint64_t total_recv_bytes, std::uint64_t err_code);

        struct FragmentSaver {
           private:
            std::shared_ptr<void> arg;
            SaveFragmentCallback callback = nullptr;

           public:
            void set(std::shared_ptr<void>&& a, SaveFragmentCallback cb) {
                if (cb) {
                    arg = std::move(a);
                    callback = cb;
                }
                else {
                    arg = nullptr;
                    callback = nullptr;
                }
            }

            std::pair<bool, error::Error> save(StreamID id, FrameType type, Fragment frag, std::uint64_t total_recv, std::uint64_t error_code) {
                if (callback) {
                    return callback(arg, id, type, frag, total_recv, error_code);
                }
                return {false, error::none};
            }
        };

        template <class Lock>
        struct RecvUniStream {
           private:
            RecvUniStreamBase<Lock> uni;
            std::weak_ptr<ConnFlowControl<Lock>> conn;
            FragmentSaver saver;
            std::shared_ptr<ack::ACKLostRecord> max_data_wait;
            std::shared_ptr<ack::ACKLostRecord> stop_sending_wait;

            IOResult send_stop_sending_unlocked(frame::fwriter& w, auto&& observer_vec) {
                if (stop_sending_wait) {
                    if (stop_sending_wait->is_lost()) {
                        if (auto res = uni.send_stop_sending(w); res != IOResult::ok) {
                            return res;
                        }
                        stop_sending_wait = ack::make_ack_wait();
                        observer_vec.push_back(stop_sending_wait);
                        return IOResult::ok;
                    }
                    if (stop_sending_wait->is_ack()) {
                        ack::put_ack_wait(std::move(stop_sending_wait));
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
                        max_data_wait = ack::make_ack_wait();
                        observer_vec.push_back(max_data_wait);
                    }
                    else if (max_data_wait->is_ack()) {
                        ack::put_ack_wait(std::move(max_data_wait));
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
                auto [_, err] = uni.recv_stream(c->base, frame, [&](Fragment frag, std::uint64_t recv_bytes) -> error::Error {
                    auto [all_done, err] = saver.save(uni.id, frame.type.type_detail(), frag, recv_bytes, 0);
                    if (err) {
                        return err;
                    }
                    if (all_done) {
                        // if all_done returns in illegal state
                        // this function doesn't change state to RecvState::data_recved
                        uni.state.recved();
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
                if (err) {
                    return err;
                }
                auto [tmp2, err2] = saver.save(uni.id, FrameType::RESET_STREAM, {.offset = uni.state.recv_bytes(), .fin = true}, uni.state.recv_bytes(), uni.state.get_error_code());
                return err2;
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

            constexpr std::uint64_t get_error_code() const {
                const auto locked = uni.lock();
                return uni.state.get_error_code();
            }

            // update recv limit
            // decide_new_limit is std::uint64_t(Limiter current)
            // if decide_new_limit returns number larger than current.curlimit(),
            // window will be increased
            bool update_recv_limit(auto&& decide_new_limit) {
                const auto locked = uni.lock();
                return uni.update_recv_limit(decide_new_limit);
            }

            // request stop sending
            bool request_stop_sending(std::uint64_t code) {
                const auto locked = uni.lock();
                return uni.request_stop_sending(code);
            }

            // set receiver user callback
            bool set_receiver(std::shared_ptr<void> arg, SaveFragmentCallback save) {
                const auto locked = uni.lock();
                if (uni.state.is_pre_recv()) {
                    saver.set(std::move(arg), save);
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

            // check whetehr stream closed
            bool is_closed() {
                const auto locked = uni.lock();
                return conn.expired() || uni.state.is_terminal_state();
            }

            // report whether stream is removable from conn
            // called by connections remove methods
            bool is_removable() {
                const auto locked = uni.lock();
                return conn.expired() ||
                       // when all data has been received,
                       // this stream is removable even if user does not read any data
                       uni.state.is_recv_all_state();
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

            bool is_closed() {
                return sender.is_closed() && receiver.is_closed();
            }

            bool is_removable() {
                return sender.is_removable() && receiver.is_removable();
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace utils

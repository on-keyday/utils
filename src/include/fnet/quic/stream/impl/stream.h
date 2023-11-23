/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream - connection implementaion
#pragma once
#include "conn_flow.h"
#include "../core/stream_base.h"
#include "../../../std/list.h"
#include "../../../std/vector.h"
#include "../../../storage.h"
#include "../../ack/ack_lost_record.h"
#include <algorithm>
#include "../../resend/fragments.h"
#include "buf_interface.h"
#include "../../resend/ack_handler.h"

namespace utils {
    namespace fnet::quic::stream::impl {

        template <class TConfig>
        struct SendUniStream {
           private:
            core::SendUniStreamBase<TConfig> uni;
            std::weak_ptr<ConnFlowControl<TConfig>> conn;

            using SendBuffer = typename TConfig::stream_handler::send_buf;

            // SendBuffer src;
            SendBufInterface<SendBuffer> src;

            resend::ACKHandler reset_wait;
            resend::Retransmiter<resend::StreamFragment, TConfig::template retransmit_que> frags;

            // move state to data_recved if condition satisfied
            void maybe_send_done() {
                if (uni.state.is_data_sent() && frags.remain() == 0) {
                    uni.all_send_done();
                }
            }

            // do STREAM frame retransmission
            IOResult send_retransmit(auto&& observer, frame::fwriter& w) {
                if (!uni.state.can_retransmit()) {
                    return IOResult::not_in_io_state;
                }
                auto res = frags.retransmit(observer, [&](resend::StreamFragment& frag, auto&& save_new) {
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

            IOResult send_stream_unlocked(frame::fwriter& w, auto&& observer) {
                auto c = conn.lock();
                if (!c) {
                    return IOResult::fatal;  // no way to recover
                }
                size_t blocked_size;
                // first try retransmition
                IOResult res = send_retransmit(observer, w);
                if (res != IOResult::ok && res != IOResult::no_data &&
                    res != IOResult::not_in_io_state) {
                    return res;
                }
                // send STREAM frame
                std::tie(res, blocked_size) = uni.send_stream(
                    c->base, w,
                    src.fairness_limit(), [&](Fragment frag) {
                        frags.sent(observer, resend::StreamFragment{
                                                 .offset = frag.offset,
                                                 .data = frag.fragment,
                                                 .fin = frag.fin,
                                             });
                        return true;
                    });
                if (res == IOResult::block_by_stream) {
                    res = core::send_stream_blocked(w, uni.id, blocked_size);
                    return res == IOResult::ok ? IOResult::block_by_stream : res;
                }
                if (res == IOResult::block_by_conn) {
                    res = core::send_conn_blocked(w, blocked_size);
                    return res == IOResult::ok ? IOResult::block_by_conn : res;
                }
                return res;
            }

            IOResult send_reset_unlocked(frame::fwriter& w, auto&& observer) {
                if (reset_wait.not_confirmed()) {
                    if (reset_wait.is_ack()) {
                        reset_wait.confirm();
                        return uni.reset_done();
                    }
                    if (reset_wait.is_lost()) {
                        auto res = uni.send_retransmit_reset(w);
                        if (res == IOResult::ok) {
                            reset_wait.wait(observer);
                        }
                        return res;
                    }
                    return IOResult::ok;  // nothing to do
                }
                auto r = uni.send_reset_if_stop_required(w);
                if (r != IOResult::ok) {
                    return r;
                }
                reset_wait.wait(observer);
                return IOResult::ok;
            }

           public:
            SendUniStream(StreamID id, std::shared_ptr<ConnFlowControl<TConfig>> c)
                : uni(id) {
                conn = c;
                uni.set_initial(c->base.state.send_ini_limit, c->base.state.local_origin());
            }

            // called when initial limits updated
            void apply_peer_initial_limits() {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return;  // no meaning
                }
                uni.set_initial(c->base.state.send_ini_limit, c->base.state.local_origin());
            }

            // each methods
            // for test or customized runtime call

            // do ack and lost detection
            // IOResult::ok - to be active
            // IOResult::no_data - not to be active
            // IOResult::not_in_io_state - finished
            IOResult detect_ack_lost() {
                const auto lock = uni.lock();
                if (reset_wait.not_confirmed()) {
                    if (reset_wait.is_ack()) {
                        uni.reset_done();
                        return IOResult::not_in_io_state;
                    }
                    if (reset_wait.is_lost()) {
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

            IOResult send_stream(frame::fwriter& w, auto&& observer) {
                const auto lock = uni.lock();
                return send_stream_unlocked(w, observer);
            }

            IOResult send_reset(frame::fwriter& w, auto&& observer) {
                const auto lock = uni.lock();
                return send_reset_unlocked(w, observer);
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
            std::pair<IOResult, bool> send(frame::fwriter& w, auto&& observer) {
                src.send_callback(this);
                const auto lock = uni.lock();
                if (uni.state.is_terminal_state()) {
                    return {IOResult::ok, true};
                }
                // fire reset if reset sent
                auto res = send_reset_unlocked(w, observer);
                if (res != IOResult::not_in_io_state) {
                    return {res, uni.state.is_terminal_state()};
                }
                res = send_stream_unlocked(w, observer);
                return {res, uni.state.is_terminal_state()};
            }

            bool recv(const frame::Frame& frame) {
                const auto callback = helper::defer([&] {
                    src.recv_callback(this);
                });
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

           private:
            void append_data(core::StreamWriteBufferState& s, auto&&... data) {
                s.perfect_written_buffer_count = 0;
                s.written_size_of_last_buffer = 0;
                s.result = IOResult::ok;
                src.append(s, std::forward<decltype(data)>(data)...);
            }

            core::StreamWriteBufferState discard_written_data_impl(bool fin, auto&&... data) {
                auto written = uni.data.written_offset;
                auto discarded = uni.data.discarded_offset;
                if (written - discarded) {
                    src.shift_front(written - discarded);
                }
                core::StreamWriteBufferState res;
                append_data(res, data...);
                if (res.result == IOResult::fatal) {
                    return res;
                }
                if (fin) {
                    src.shrink_to_fit();
                }
                res.result = uni.update_data(written, src, fin) ? res.result : IOResult::fatal;  // must success
                return res;
            }

           public:
            // discard_written_data discards written stream data from runtime buffer
            bool discard_written_data() {
                const auto locked = uni.lock();
                return discard_written_data_impl(uni.data.fin).result != IOResult::fatal;
            }

            // add_multi_data adds multiple data
            // this is better performance than multiple call of add_data because of reduction of lock.
            // see also description of add_data to know about parameters
            core::StreamWriteBufferState add_multi_data(bool fin, bool discard_written, auto&&... data) {
                const auto locked = uni.lock();
                if (uni.data.fin) {
                    if (discard_written) {
                        if (auto res = discard_written_data_impl(true); res.result != IOResult::ok) {
                            return res;
                        }
                    }
                    return core::StreamWriteBufferState{.result = IOResult::cancel};
                }
                if (discard_written) {
                    return discard_written_data_impl(fin, std::forward<decltype(data)>(data)...);
                }
                core::StreamWriteBufferState res;
                append_data(res, std::forward<decltype(data)>(data)...);
                if (res.result == IOResult::fatal) {
                    return res;
                }
                res.result = uni.update_data(uni.data.discarded_offset, src, fin) ? res.result : IOResult::fatal;  // must success
                return res;
            }

            // add send data
            // if fin is true, FIN flag is set and later call of this returns IOResult::cancel
            // if discard_written is true, discard written data from runtime buffer
            // you can call this with discard_written = true to discard buffer even if fin is set
            // but use discard_written_data() instead to discard only.
            core::StreamWriteBufferState add_data(view::rvec data, bool fin = false, bool discard_written = false) {
                return add_multi_data(fin, discard_written, data);
            }

            // set sender user level handler
            bool set_sender(SendBuffer&& handler) {
                const auto lock = uni.lock();
                if (uni.state.is_ready()) {
                    src.impl = std::move(handler);
                    return true;
                }
                return false;
            }

            auto get_sender_ctx() {
                const auto lock = uni.lock();
                return src.get_specific();
            }

            // request cancelation
            IOResult request_reset(std::uint64_t code) {
                const auto locked = uni.lock();
                return uni.request_reset(code);
            }

            // check whether provided data completely transmited
            bool data_transmited() {
                const auto locked = uni.lock();
                return uni.data.remain() == 0 && frags.remain() == 0;
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

            // is_fin reports whether fin flag is set
            bool is_fin() {
                const auto locked = uni.lock();
                return uni.data.fin;
            }

            auto access_flow_limit(auto&& access) {
                const auto locked = uni.lock();
                return access(uni.state.get_send_limiter());
            }
        };

        template <class TConfig>
        struct RecvUniStream {
           private:
            core::RecvUniStreamBase<TConfig> uni;
            std::weak_ptr<ConnFlowControl<TConfig>> conn;
            using RecvBuffer = typename TConfig::stream_handler::recv_buf;
            // RecvBuffer saver;
            RecvBufInterface<RecvBuffer> saver;
            resend::ACKHandler max_data_wait;
            resend::ACKHandler stop_sending_wait;

            IOResult send_stop_sending_unlocked(frame::fwriter& w, auto&& observer) {
                if (stop_sending_wait.not_confirmed()) {
                    if (stop_sending_wait.is_lost()) {
                        if (auto res = uni.send_stop_sending(w); res != IOResult::ok) {
                            return res;
                        }
                        stop_sending_wait.wait(observer);
                        return IOResult::ok;
                    }
                    if (stop_sending_wait.is_ack()) {
                        stop_sending_wait.confirm();
                        return IOResult::ok;
                    }
                }
                // try to send STOP_SENDING
                // returns IOResult::not_in_io_state if not in state
                if (auto res = uni.send_stop_sending(w); res != IOResult::ok) {
                    return res;
                }
                stop_sending_wait.wait(observer);
                return IOResult::ok;
            }

            IOResult send_max_stream_data_unlocked(frame::fwriter& w, auto&& observer) {
                auto res = uni.send_max_stream_data_if_updated(w);
                if (res == IOResult::ok) {
                    // clear old ACKLostRecord
                    max_data_wait.wait(observer);
                    return res;
                }
                if (res == IOResult::fatal) {
                    return res;
                }
                if (max_data_wait.not_confirmed()) {
                    if (max_data_wait.is_lost()) {
                        // retransmit
                        res = uni.send_max_stream_data(w);
                        if (res != IOResult::ok) {
                            if (res == IOResult::not_in_io_state) {
                                // state is not RecvState::Recv
                                max_data_wait.confirm();
                            }
                            return res;
                        }
                        max_data_wait.wait(observer);
                    }
                    else if (max_data_wait.is_ack()) {
                        max_data_wait.confirm();
                    }
                }
                return IOResult::ok;
            }

            void set_initial(InitialLimits& recv_ini_limit, Origin self) {
                uni.set_initial(recv_ini_limit, self);
            }

           public:
            RecvUniStream(StreamID id, std::shared_ptr<ConnFlowControl<TConfig>> c)
                : uni(id) {
                conn = c;
                set_initial(c->base.state.recv_ini_limit, c->base.state.local_origin());
            }

            void apply_local_initial_limits() {
                const auto locked = uni.lock();
                auto c = conn.lock();
                if (!c) {
                    return;  // no meaning
                }
                set_initial(c->base.state.recv_ini_limit, c->base.state.local_origin());
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
                auto [tmp2, err2] = saver.save(uni.id, FrameType::RESET_STREAM, Fragment{.offset = uni.state.recv_bytes(), .fin = true}, uni.state.recv_bytes(), uni.state.get_error_code());
                return err2;
            }

            IOResult send_stop_sending(frame::fwriter& w, auto&& observer) {
                const auto locked = uni.lock();
                return send_stop_sending_unlocked(w, observer);
            }

            IOResult send_max_stream_data(frame::fwriter& w, auto&& observer) {
                const auto locked = uni.lock();
                return send_stop_sending_unlocked(w, observer);
            }

            // general handling
            // called by QUIC runtime system

            error::Error recv(const frame::Frame& frame) {
                const auto callback = helper::defer([&] {
                    saver.recv_callback(this);
                });
                if (is_STREAM(frame.type.type_detail())) {
                    return recv_stream(static_cast<const frame::StreamFrame&>(frame));
                }
                if (frame.type.type_detail() == FrameType::RESET_STREAM) {
                    return recv_reset(static_cast<const frame::ResetStreamFrame&>(frame));
                }
                if (frame.type.type_detail() == FrameType::STREAM_DATA_BLOCKED) {
                    return error::none;
                }
                return error::Error("unexpected frame type", error::Category::lib, error::fnet_quic_implementation_bug);
            }

            IOResult send(frame::fwriter& w, auto&& observer) {
                saver.send_callback(this);
                const auto locked = uni.lock();
                auto res = send_stop_sending_unlocked(w, observer);
                if (res == IOResult::fatal) {
                    return res;
                }
                return send_max_stream_data_unlocked(w, observer);
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
            // if decide_new_limit returns number larger than current.current_limit(),
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

            // set user level recv handler
            bool set_receiver(RecvBuffer&& handler) {
                const auto locked = uni.lock();
                if (uni.state.is_pre_recv()) {
                    saver.impl = std::move(handler);
                    return true;
                }
                return false;
            }

            auto get_receiver_ctx() {
                const auto locked = uni.lock();
                return saver.get_specific();
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

        template <class TConfig>
        struct BidiStream {
            SendUniStream<TConfig> sender;
            RecvUniStream<TConfig> receiver;

            // general handling
            // called by QUIC runtime system

            // returns (result,finished)
            // called by quic runtime
            std::pair<IOResult, bool> send(frame::fwriter& w, auto&& observer) {
                auto res = receiver.send(w, observer);
                if (res == IOResult::fatal || res == IOResult::invalid_data) {
                    return {res, false};
                }
                return sender.send(w, observer);
            }

            // do recv for each sender or receiver
            // called by quic runtime
            error::Error recv(const frame::Frame& frame) {
                if (sender.recv(frame)) {
                    return error::none;
                }
                return receiver.recv(frame);
            }

            void apply_peer_initial_limits() {
                sender.apply_peer_initial_limits();
            }

            void apply_local_initial_limits() {
                receiver.apply_local_initial_limits();
            }

            BidiStream(StreamID id,
                       std::shared_ptr<ConnFlowControl<TConfig>> conn)
                : sender(id, conn), receiver(id, conn) {}

            constexpr StreamID id() const {
                return sender.id();
            }

            bool is_closed() {
                return sender.is_closed() && receiver.is_closed();
            }

            bool is_removable() {
                return sender.is_removable() && receiver.is_removable();
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace utils

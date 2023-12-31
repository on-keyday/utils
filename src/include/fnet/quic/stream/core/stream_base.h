/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream - stream object
#pragma once
#include "conn_base.h"
#include "../../frame/stream_manage.h"
#include "../fragment.h"
#include "../../ioresult.h"
#include "../../frame/writer.h"
#include "../stream_id.h"

namespace utils {
    namespace fnet::quic::stream::core {

        struct StreamWriteData {
            view::rvec src;
            std::uint64_t discarded_offset = 0;
            // OFFSET of stream
            std::uint64_t written_offset = 0;
            bool fin = false;
            bool fin_written = false;

            // written_offset < src.size()+discarded_offset
            // written_offset - discarded_offset < src.size()
            constexpr view::rvec get_remain(size_t limit) {
                return src.substr(written_offset - discarded_offset, limit);
            }

            constexpr bool update(std::uint64_t new_discard, view::rvec new_src, bool is_fin) {
                if (
                    // if finish sending,size mismatch is an error
                    (fin ? (src.size() + discarded_offset != new_src.size() + new_discard)
                         // otherwise smaller size is an error
                         : (src.size() + discarded_offset > new_src.size() + new_discard)) ||
                    // check whether data before new_discard offset is already sent
                    new_discard > written_offset) {
                    return false;
                }
                src = new_src;
                discarded_offset = new_discard;
                fin = fin || is_fin;
                return true;
            }

            constexpr size_t remain() const noexcept {
                return src.size() + discarded_offset - written_offset;
            }

            constexpr bool should_fin() const noexcept {
                return fin && !fin_written;
            }

            constexpr void write_fin() noexcept {
                fin_written = true;
            }
        };

        // returns (result,blocked_limit_if_blocked)
        // faireness_limit is limit usage for other stream usage
        template <class TConfig>
        constexpr std::pair<IOResult, std::uint64_t> send_impl(
            frame::fwriter& w, ConnectionBase<TConfig>& conn, StreamID id, std::uint64_t faireness_limit,
            SendUniStreamState& state, StreamWriteData& data, auto&& save_fragment) {
            if (!state.can_send()) {
                return {IOResult::not_in_io_state, 0};  // cannot sendable on this state
            }

            const auto writable_size = w.remain().size();
            const auto least_overhead = frame::calc_stream_overhead(id, data.written_offset, data.written_offset != 0);
            if (writable_size < least_overhead) {
                return {IOResult::no_capacity, 0};  // writable doesn't have enough capacity even least overhead
            }
            const auto remain_data_size = data.remain();
            if (remain_data_size == 0 && !data.should_fin()) {
                return {IOResult::no_data, 0};  // nothing to write
            }

            // adjust payload size for fairness among streams
            const auto adjust_data_size = remain_data_size < faireness_limit ? remain_data_size : faireness_limit;

            const auto stream_flow_limit = state.sendable_size();
            // decide stream payload size temporary
            const auto payload_limit = adjust_data_size < stream_flow_limit ? adjust_data_size : stream_flow_limit;
            if (payload_limit == 0 && !data.should_fin()) {
                return {IOResult::block_by_stream, state.send_limit()};  // blocked by stream flow control limit
            }

            bool ok = true, block = false;
            frame::StreamFrame frame;
            std::uint64_t max_conn_data = 0;
            conn.use_send_credit([&](std::uint64_t conn_limit, std::uint64_t max_data) -> std::uint64_t {
                max_conn_data = max_data;
                // now enter lock, do fast!
                const std::uint64_t flow_control_limit = conn_limit < payload_limit ? conn_limit : payload_limit;
                if (flow_control_limit == 0 && !data.should_fin()) {
                    block = true;
                    return 0;  // now connection level flow control limit
                }
                // make stream frame
                auto stream_data = data.get_remain(flow_control_limit);
                std::tie(frame, ok) = frame::make_fit_size_Stream(writable_size, id, data.written_offset, stream_data,
                                                                  data.should_fin(), data.should_fin());
                return ok ? frame.stream_data.size() : 0;  // use connection flow
            });
            if (block) {
                return {IOResult::block_by_conn, max_conn_data};  // blocked by flow control
            }
            if (!ok) {
                // no way to deliver because of lack of writable_size for frame overhead
                // wait for next chance.
                return {IOResult::no_capacity, 0};
            }

            // update send state
            if (!state.send(frame.offset + frame.stream_data.size())) {
                return {IOResult::fatal, 0};  // fatal error! library bug!!
            }
            // update written offset
            data.written_offset += frame.stream_data.size();
            if (!w.write(frame)) {
                return {IOResult::fatal, 0};  // fatal error! library bug!!
            }

            // add fragment
            Fragment frag;
            if (frame.type.STREAM_fin()) {
                // TODO(on-keyday): refactor state and data to make meaning more clearly?
                state.sent();      // update state
                data.write_fin();  // update data
                frag.fin = true;
            }
            frag.fragment = frame.stream_data;
            frag.offset = frame.offset;
            // TODO(on-keyday): cb should be called in conn.send() callback?
            if (!save_fragment(frag)) {
                return {IOResult::fatal, 0};
            }
            return {IOResult::ok, 0};
        }

        template <class TConfig>
        struct SendUniStreamBase {
            using Lock = typename TConfig::send_stream_lock;
            Lock locker;
            const StreamID id = invalid_id;
            SendUniStreamState state;
            StreamWriteData data;

            constexpr SendUniStreamBase(StreamID id)
                : id(id) {
                assert(this->id.valid());
            }

            constexpr auto lock() {
                return helper::lock(locker);
            }

            constexpr bool update_data(std::uint64_t discard, view::rvec src, bool fin) {
                return data.update(discard, src, fin);
            }

            constexpr bool has_data_to_send() const {
                return state.can_send() &&
                       data.remain() != 0;
            }

            // settings by peer
            constexpr void set_initial(const InitialLimits& ini, Origin self) {
                if (id.type() == StreamType::uni) {
                    state.set_send_limit(ini.uni_stream_data_limit);
                }
                else if (id.type() == StreamType::bidi) {
                    if (id.origin() == self) {
                        state.set_send_limit(ini.bidi_stream_data_remote_limit);
                    }
                    else {
                        state.set_send_limit(ini.bidi_stream_data_local_limit);
                    }
                }
            }

            template <class ConnLock>
            constexpr std::pair<IOResult, std::uint64_t> send_stream(ConnectionBase<ConnLock>& conn, frame::fwriter& w, std::uint64_t faireness_limit, auto&& save_fragment) {
                return send_impl(w, conn, id, faireness_limit, state, data, save_fragment);
            }

            // return false is no error
            constexpr bool recv_max_stream_data(const frame::MaxStreamDataFrame& frame) {
                if (id != frame.streamID) {
                    return false;
                }
                return state.update_send_limit(frame.max_stream_data);
            }

            constexpr IOResult send_reset(frame::fwriter& w) noexcept {
                frame::ResetStreamFrame reset;
                reset.streamID = id.to_int();
                reset.application_protocol_error_code = state.get_error_code();
                reset.final_size = state.sent_bytes();
                if (w.remain().size() < reset.len()) {
                    return IOResult::no_capacity;  // unable to write
                }
                if (!state.reset()) {
                    return IOResult::not_in_io_state;
                }
                return w.write(reset) ? IOResult::ok : IOResult::fatal;  // must success
            }

            constexpr IOResult send_reset_if_stop_required(frame::fwriter& w) noexcept {
                if (state.is_reset_required()) {
                    return send_reset(w);
                }
                return IOResult::not_in_io_state;
            }

            constexpr IOResult send_retransmit_reset(frame::fwriter& w) {
                if (!state.can_retransmit_reset()) {
                    return IOResult::not_in_io_state;
                }
                frame::ResetStreamFrame reset;
                reset.streamID = id.to_int();
                reset.application_protocol_error_code = state.get_error_code();
                reset.final_size = state.sent_bytes();
                if (w.remain().size() < reset.len()) {
                    return IOResult::no_capacity;  // unable to write
                }
                return w.write(reset) ? IOResult::ok : IOResult::fatal;  // must success
            }

            constexpr void recv_stop_sending(const frame::StopSendingFrame& frame) {
                if (frame.streamID != id) {
                    return;
                }
                state.recv_stop_sending(frame.application_protocol_error_code);
            }

            constexpr IOResult request_reset(std::uint64_t code) {
                return state.set_error_code(code) ? IOResult::ok : IOResult::not_in_io_state;
            }

            constexpr bool all_send_done() {
                return state.sent_ack();
            }

            constexpr IOResult reset_done() {
                if (!state.reset_ack()) {
                    return IOResult::not_in_io_state;
                }
                return IOResult::ok;
            }

            // save_retransmit is bool(sent,remain,has_remain)
            constexpr IOResult send_retransmit_stream(frame::fwriter& w, std::uint64_t offset, view::rvec fragment, bool fin, auto&& save_retransmit) {
                if (!state.can_retransmit()) {
                    return IOResult::not_in_io_state;
                }
                if (offset + fragment.size() > data.written_offset) {
                    return IOResult::invalid_data;
                }
                if (fin) {
                    if (!data.fin_written) {
                        return IOResult::invalid_data;
                    }
                    if (offset + fragment.size() != data.written_offset) {
                        return IOResult::invalid_data;
                    }
                }
                else {
                    if (offset + fragment.size() > data.written_offset) {
                        return IOResult::invalid_data;
                    }
                }
                auto [frame, ok] = frame::make_fit_size_Stream(w.remain().size(), id, offset, fragment, fin, fin);
                if (!ok) {
                    return IOResult::no_capacity;
                }
                if (!w.write(frame)) {
                    return IOResult::fatal;
                }
                Fragment frag, remain;
                frag.offset = frame.offset;
                frag.fragment = frame.stream_data;
                frag.fin = frame.type.STREAM_fin();
                remain.offset = frame.offset + frame.stream_data.size();
                remain.fragment = fragment.substr(frame.stream_data.size());
                remain.fin = fin && !frame.type.STREAM_fin();
                const bool remaining = remain.fragment.size() != 0;
                if (!save_retransmit(frag, remain, remaining)) {
                    return IOResult::fatal;
                }
                return IOResult::ok;
            }
        };

        // you should send after send() returns Result::block_by_stream
        constexpr IOResult send_stream_blocked(frame::fwriter& w, StreamID id, std::uint64_t max_data) noexcept {
            // check value bordr
            if (max_data >= varint::border(8) || !id.valid()) {
                return IOResult::invalid_data;
            }
            frame::StreamDataBlockedFrame blocked;
            blocked.streamID = id.to_int();
            blocked.max_stream_data = max_data;
            if (w.remain().size() < blocked.len()) {
                return IOResult::no_capacity;
            }
            return w.write(blocked) ? IOResult::ok : IOResult::fatal;  // must success
        }

        // returns (result,err_if_fatal)
        template <class TConfig>
        constexpr std::pair<IOResult, error::Error> recv_impl(
            const frame::StreamFrame& frame, ConnectionBase<TConfig>& conn,
            StreamID id, RecvUniStreamState& state,
            auto&& deliver_data) {
            if (frame.streamID != id) {
                return {IOResult::id_mismatch, error::Error("unexpected streamID on StreamFrame. library bug!!", error::Category::lib, error::fnet_quic_implementation_bug)};  // library bug
            }
            if (!state.can_recv()) {
                return {IOResult::not_in_io_state, error::none};  // ignore frame
            }
            std::uint64_t prev_used = state.recv_bytes();
            if (!state.recv(frame.offset + frame.stream_data.size())) {
                return {IOResult::block_by_stream,
                        QUICError{
                            .msg = "stream flow control limit overflow",
                            .transport_error = TransportError::FLOW_CONTROL_ERROR,
                            .frame_type = frame.type.type_detail(),
                        }};
            }
            std::uint64_t cur_used = state.recv_bytes();
            if (cur_used - prev_used) {  // new usage exists
                // update connection limit usage
                if (!conn.use_recv_credit(cur_used - prev_used)) {
                    return {
                        IOResult::block_by_conn,
                        QUICError{
                            .msg = "connection flow control limit overflow",
                            .transport_error = TransportError::FLOW_CONTROL_ERROR,
                            .frame_type = frame.type.type_detail(),
                        }};
                }
            }
            if (frame.type.STREAM_fin()) {
                state.size_known();  // make stream size known
            }
            Fragment frag;
            frag.offset = frame.offset;
            frag.fragment = frame.stream_data;
            frag.fin = frame.type.STREAM_fin();
            if (error::Error err = deliver_data(frag, cur_used)) {
                // delivery error
                return {
                    IOResult::fatal,
                    err,
                };
            }
            return {IOResult::ok, error::none};
        }

        template <class TConfig>
        struct RecvUniStreamBase {
            using Lock = typename TConfig::recv_stream_lock;
            Lock locker;
            const StreamID id;
            RecvUniStreamState state;

            constexpr RecvUniStreamBase(StreamID id)
                : id(id) {}

            constexpr auto lock() {
                return helper::lock(locker);
            }

            // settings by local
            constexpr void set_initial(const InitialLimits& ini, Origin self) {
                if (id.type() == StreamType::uni) {
                    state.set_recv_limit(ini.uni_stream_data_limit);
                }
                else if (id.type() == StreamType::bidi) {
                    if (id.origin() == self) {
                        state.set_recv_limit(ini.bidi_stream_data_local_limit);
                    }
                    else {
                        state.set_recv_limit(ini.bidi_stream_data_remote_limit);
                    }
                }
            }

            template <class ConnLock>
            constexpr std::pair<IOResult, error::Error> recv_stream(ConnectionBase<ConnLock>& conn, const frame::StreamFrame& frame, auto&& deliver_data) {
                return recv_impl(frame, conn, id, state, deliver_data);
            }

            constexpr bool update_recv_limit(auto&& decide_new_limit) {
                std::uint64_t new_limit = decide_new_limit(state.get_recv_limiter(), state.initial_limit());
                // update limit (ignore result)
                state.update_recv_limit(new_limit);
                return true;
            }

            constexpr IOResult send_max_stream_data(frame::fwriter& w) {
                if (!state.is_recv()) {
                    return IOResult::not_in_io_state;
                }
                // decide new limit by user callback
                frame::MaxStreamDataFrame frame;
                frame.streamID = id.to_int();
                frame.max_stream_data = state.recv_limit();
                // check capcacity
                if (w.remain().size() < frame.len()) {
                    return IOResult::no_capacity;
                }
                // render must success
                if (!w.write(frame)) {
                    return IOResult::fatal;
                }
                state.max_stream_data_sent();  // clear primary send flag
                return IOResult::ok;
            }

            constexpr IOResult send_max_stream_data_if_updated(frame::fwriter& w) {
                if (!state.require_send_limit_update()) {
                    return IOResult::not_in_io_state;
                }
                return send_max_stream_data(w);
            }

            template <class ConnLock>
            constexpr std::pair<IOResult, error::Error> recv_reset(ConnectionBase<ConnLock>& conn, const frame::ResetStreamFrame& reset) {
                // check id
                if (id != reset.streamID) {
                    return {IOResult::id_mismatch, error::Error("unexpected stream id on ResetFrame. library bug!!", error::Category::lib, error::fnet_quic_implementation_bug)};  // libary bug!!
                }

                auto prev_used = state.recv_bytes();
                // update state
                if (!state.reset(reset.final_size, reset.application_protocol_error_code)) {
                    return {IOResult::block_by_stream,
                            QUICError{
                                .msg = "final size has no consistency",
                                .transport_error = TransportError::FINAL_SIZE_ERROR,
                                .frame_type = FrameType::RESET_STREAM,
                            }};
                }
                auto cur_used = state.recv_bytes();
                if (cur_used - prev_used) {  // new usage exists
                    if (!conn.use_recv_credit(cur_used - prev_used)) {
                        return {
                            IOResult::block_by_stream,
                            QUICError{
                                .msg = "connection flow control limit violation",
                                .transport_error = TransportError::FLOW_CONTROL_ERROR,
                                .frame_type = FrameType::RESET_STREAM,
                            }};
                    }
                }
                return {IOResult::ok, error::none};
            }

            constexpr bool request_stop_sending(std::uint64_t code) {
                return state.stop_sending(code);
            }

            constexpr IOResult send_stop_sending(frame::fwriter& w) {
                if (!state.is_stop_required() || !state.can_stop_sending()) {
                    return IOResult::not_in_io_state;
                }
                frame::StopSendingFrame frame;
                frame.streamID = id.to_int();
                frame.application_protocol_error_code = state.get_error_code();
                if (w.remain().size() < frame.len()) {
                    return IOResult::no_capacity;
                }
                return w.write(frame) ? IOResult::ok : IOResult::fatal;  // must success
            }
        };

        namespace test {
            struct TestConfig {
                using conn_lock = EmptyLock;
                using send_stream_lock = EmptyLock;
            };

            constexpr bool check_stream_send() {
                using Config = TestConfig;
                SendUniStreamBase<Config> base{1};
                ConnectionBase<Config> conn;
                byte data[63];
                binary::writer w{data};
                conn.state.conn_flow.send.update_limit(1000);
                base.state.update_send_limit(1000);
                byte src[11000]{};
                base.data.src = src;
                frame::fwriter fw{w};
                auto [result, ig1] = base.send_stream(conn, fw, ~0, [](Fragment frag) {
                    return frag.offset == 0 && frag.fragment.size() == 61;
                });
                if (result != IOResult::ok) {
                    return false;
                }
                base.data.src = view::wvec(src, 63);
                w.reset();
                auto [result1, ig3] = base.send_stream(conn, fw, ~0, [](Fragment frag) {
                    return frag.offset == 61 && frag.fragment.size() == 2;
                });
                (void)ig3;
                return result1 == IOResult::ok;
            }

            static_assert(check_stream_send());
        }  // namespace test

    }  // namespace fnet::quic::stream::core
}  // namespace utils

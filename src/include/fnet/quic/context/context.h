/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// context - quic context suite
#pragma once
#include "../ack/sent_history.h"
#include "../ack/unacked.h"
#include "../status/status.h"
#include "../packet/creation.h"
#include "../packet/parse.h"
#include "../connid/id_manage.h"
#include "../path/dplpmtud.h"
#include "../path/verify.h"
#include "../crypto/suite.h"
#include "../crypto/crypto_tag.h"
#include "../crypto/callback.h"
#include "../crypto/crypto.h"
#include "../crypto/decrypt.h"
#include "../transport_parameter/suite.h"
#include "../frame/parse.h"
#include "../stream/impl/conn.h"
#include "../log/log.h"
#include "../../std/deque.h"
#include "../token/token.h"
#include "../close/close.h"
#include "config.h"
#include <atomic>

namespace utils {
    namespace fnet::quic::context {

        enum class WriteMode {
            none = 0x0,
            use_all_buffer = 0x1,
            mtu_probe = 0x2,
            conn_close = 0x4,
        };

        constexpr WriteMode operator|(const WriteMode& a, const WriteMode& b) noexcept {
            return WriteMode(std::uint32_t(a) | std::uint32_t(b));
        }

        constexpr bool operator&(const WriteMode& a, const WriteMode& b) noexcept {
            return std::uint32_t(a) & std::uint32_t(b);
        }

        constexpr auto err_creation = error::Error("error on creating packet. library bug!!");
        constexpr auto err_idle_timeout = error::Error("IDLE_TIMEOUT");
        constexpr auto err_handshake_timeout = error::Error("HANDSHAKE_TIMEOUT");

        // Context is QUIC connection context suite
        //
        template <class Lock, class CongestionAlgorithm = status::NewReno>
        struct Context {
           private:
            Version version = version_negotiation;
            status::Status<CongestionAlgorithm> status;
            trsparam::TransportParameters params;
            crypto::CryptoSuite crypto;
            ack::SentPacketHistory ackh;
            ack::UnackedPackets unacked;
            connid::IDManager connIDs;
            token::RetryToken retry_token;
            path::MTU mtu;
            path::PathVerifier<slib::deque> path_verifyer;
            std::shared_ptr<stream::impl::Conn<Lock>> streams;
            close::Closer closer;
            std::atomic<close::CloseReason> closed;
            Lock close_lock;

            log::Logger logger;

            // internal parameters
            flex_storage packet_creation_buffer;
            bool transport_param_read = false;
            byte src_connID_len = 0;

            void set_quic_runtime_error(auto&& err) {
                close_lock.lock();
                const auto d = helper::defer([&] {
                    close_lock.unlock();
                });
                closer.on_error(std::move(err), close::ErrorType::runtime);
            }

            void recv_close(const frame::ConnectionCloseFrame& conn_close) {
                close_lock.lock();
                const auto d = helper::defer([&] {
                    close_lock.unlock();
                });
                if (closer.recv(conn_close)) {
                    closed.store(close::CloseReason::close_by_remote);  // remote close
                }
            }

            bool apply_transport_param(packet::PacketSummary summary) {
                if (transport_param_read) {
                    return true;
                }
                auto data = crypto.tls().get_peer_quic_transport_params();
                if (data.null()) {
                    return true;  // nothing can do
                }
                io::reader r{data};
                auto err = params.parse_peer(r, status.is_server());
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                auto& peer = params.peer;
                // get registered initial_source_connection_id by peer
                auto connID = connIDs.choose_dstID(0);
                if (peer.initial_src_connection_id.null() || peer.initial_src_connection_id != connID.id) {
                    set_quic_runtime_error(QUICError{
                        .msg = "initial_src_connection_id is not matched to id contained on initial packet",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                    });
                    return false;
                }
                if (!status.is_server()) {  // client
                    if (peer.original_dst_connection_id != connIDs.get_initial()) {
                        set_quic_runtime_error(QUICError{
                            .msg = "original_dst_connection_id is not matched to id contained on initial packet",
                            .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                        });
                        return false;
                    }
                    if (status.has_received_retry()) {
                        if (peer.retry_src_connection_id != connIDs.get_retry()) {
                            set_quic_runtime_error(QUICError{
                                .msg = "retry_src_connection_id is not matched to id previous received",
                                .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                            });
                            return false;
                        }
                    }
                    connIDs.on_initial_stateless_reset_token_received(peer.stateless_reset_token);
                    if (!peer.preferred_address.connectionID.null()) {
                        connIDs.on_preferred_address_received(version, peer.preferred_address.connectionID, peer.preferred_address.stateless_reset_token);
                    }
                    mtu.on_transport_parameter_received(peer.max_udp_payload_size);
                }
                status.on_transport_parameter_received(peer.max_idle_timeout, peer.max_ack_delay, peer.ack_delay_exponent);
                auto& local = params.local;
                auto by_peer = trsparam::to_initial_limits(peer);
                auto by_local = trsparam::to_initial_limits(local);
                streams->apply_initial_limits(by_local, by_peer);
                transport_param_read = true;
                logger.debug("apply transport parameter");
                return true;
            }

            void on_handshake_confirmed() {
                ackh.on_packet_number_space_discarded(status, status::PacketNumberSpace::handshake);
                logger.debug("drop handshake keys");
                status.on_handshake_confirmed();
                mtu.on_handshake_confirmed();
                logger.debug("handshake confirmed");
            }

            // writer methods

            // write_frames calls each send() call of frame generator
            bool write_frames(const packet::PacketSummary& summary, ack::ACKWaiters& waiters, frame::fwriter& fw) {
                auto pn_space = status::from_packet_type(summary.type);
                auto wrap_io = [&](IOResult res) {
                    set_quic_runtime_error(QUICError{
                        .msg = to_string(res),
                        .frame_type = fw.prev_type,
                    });
                    return false;
                };
                if (summary.type != PacketType::ZeroRTT) {
                    if (auto err = unacked.send(fw, pn_space)) {
                        set_quic_runtime_error(std::move(err));
                        return false;
                    }
                }
                if (status.is_probe_required(pn_space) || status.should_send_any_packet()) {
                    logger.pto_fire(pn_space);
                    fw.write(frame::PingFrame{});
                }
                if (status.should_send_ping()) {
                    fw.write(frame::PingFrame{});
                }
                if (path_verifyer.path_verification_required()) {
                    // currently path verification is required
                    // can send only
                    if (auto err = path_verifyer.send_path_challange(fw, waiters); err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                }
                if (auto err = crypto.send(summary.type, summary.packet_number, waiters, fw);
                    err == IOResult::fatal) {
                    return wrap_io(err);
                }
                if (summary.type == PacketType::OneRTT) {
                    if (auto err = path_verifyer.send_path_response(fw);
                        err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                }
                if (summary.type == PacketType::ZeroRTT ||
                    summary.type == PacketType::OneRTT) {
                    if (auto err = streams->send(fw, waiters); err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                    if (auto err = connIDs.send(fw, waiters); err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                }
                return true;
            }

            // render_payload_callback returns render_payload callback for packet::create_packet
            // this calls write_frame and add paddings for encryption
            auto render_payload_callback(packet::PacketSummary& summary, ack::ACKWaiters& wait, bool& no_payload, packet::PacketStatus& pstatus, bool fill_all) {
                return [&, fill_all](io::writer& w, packetnum::WireVal wire) {
                    const auto prev_offset = w.offset();
                    frame::fwriter fw{w};
                    if (!write_frames(summary, wait, fw)) {
                        return false;
                    }
                    if (prev_offset >= w.offset()) {
                        no_payload = true;
                        return false;
                    }
                    if (fill_all) {
                        if (!w.full()) {
                            fw.write(frame::PaddingFrame{});  // apply one
                        }
                    }
                    else {
                        frame::add_padding_for_encryption(fw, wire, crypto::sample_skip_size);
                    }
                    pstatus = fw.status;
                    logger.sending_packet(summary, w.written());  // logging
                    return true;
                };
            }

            bool save_sent_packet(packet::PacketSummary summary, packet::PacketStatus pstatus, std::uint64_t sent_bytes, ack::ACKWaiters& waiters, bool is_mtu_probe) {
                const auto pn_space = status::from_packet_type(summary.type);
                ack::SentPacket sent;
                sent.packet_number = summary.packet_number;
                sent.type = summary.type;
                sent.status = pstatus;
                sent.sent_bytes = sent_bytes;
                sent.waiters = std::move(waiters);
                sent.is_mtu_probe = is_mtu_probe;
                auto err = ackh.on_packet_sent(status, pn_space, std::move(sent));
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    ack::mark_as_lost(sent.waiters);
                    return false;
                }
                crypto.on_packet_sent(summary.type, summary.packet_number);
                return true;
            }

            bool write_packet(io::writer& w, packet::PacketSummary summary, WriteMode mode, ack::ACKWaiters& waiters) {
                auto set_error = [&](auto&& err) {
                    if (mode & WriteMode::conn_close) {
                        return;
                    }
                    set_quic_runtime_error(std::move(err));
                };
                // get packet number space
                auto pn_space = status::from_packet_type(summary.type);
                if (pn_space == status::PacketNumberSpace::no_space) {
                    set_error(error::Error("invalid packet type"));
                    return false;
                }

                // set QUIC version and packet number
                const auto [pn, largest_acked] = status.next_and_largest_acked_packet_number(pn_space);
                summary.version = version;
                summary.packet_number = pn;

                // get encryption suite
                const auto [suite, tmp_err] = crypto.enc_suite(version, summary.type, crypto::KeyMode::current);
                if (tmp_err) {
                    set_error(std::move(tmp_err));
                    return false;
                }
                if (summary.type == PacketType::OneRTT) {
                    summary.key_bit = suite.pharse == crypto::KeyPhase::one;
                }

                // define frame renderer arguments
                bool no_payload = false;
                packet::PacketStatus pstatus;
                auto mark_lost = helper::defer([&] {
                    ack::mark_as_lost(waiters);
                });

                // use copy
                // restore when payload rendered and succeeded
                auto copy = w;

                // render packet
                packet::CryptoPacket plain;
                bool res = false;

                if (mode & WriteMode::conn_close) {
                    std::tie(plain, res) = packet::create_packet(
                        copy, summary, largest_acked,
                        crypto::authentication_tag_length,
                        mode & WriteMode::use_all_buffer, [&](io::writer& w, packetnum::WireVal wire) {
                            frame::fwriter fw{w};
                            if (!closer.send(fw)) {
                                return false;
                            }
                            frame::add_padding_for_encryption(fw, wire, crypto::sample_skip_size);
                            pstatus = fw.status;
                            logger.sending_packet(summary, w.written());
                            return true;
                        });
                }
                else if (mode & WriteMode::mtu_probe) {
                    std::tie(plain, res) = packet::create_packet(
                        copy, summary, largest_acked,
                        crypto::authentication_tag_length, true, [&](io::writer& w, packetnum::WireVal wire) {
                            frame::fwriter fw{w};
                            fw.write(frame::PingFrame{});
                            pstatus = fw.status;
                            logger.sending_packet(summary, w.written());
                            return true;
                        });
                }
                else {
                    const auto fill_all = mode & WriteMode::use_all_buffer;
                    std::tie(plain, res) = packet::create_packet(
                        copy, summary, largest_acked,
                        crypto::authentication_tag_length, fill_all,
                        render_payload_callback(summary, waiters, no_payload, pstatus, fill_all));
                }

                if (!(mode & WriteMode::conn_close)) {
                    if (closer.has_error()) {
                        return false;
                    }
                }
                if (no_payload) {
                    return true;  // skip. no payload exists
                }
                if (!res) {
                    set_error(err_creation);
                    return false;
                }

                // encrypt packet
                auto err = crypto::encrypt_packet(*suite.keyiv, *suite.hp, *suite.cipher, plain);
                if (err) {
                    set_error(std::move(err));
                    return false;
                }

                mark_lost.cancel();

                if (!(mode & WriteMode::conn_close)) {
                    // save packet
                    if (!save_sent_packet(summary, pstatus, plain.src.size(), waiters, mode & WriteMode::mtu_probe)) {
                        return false;
                    }
                }

                status.consume_packet_number(pn_space);
                w = copy;  // restore

                return true;
            }

            // write_initial generates initial packet
            bool write_initial(io::writer& w, WriteMode mode, ack::ACKWaiters& waiters) {
                packet::PacketSummary summary;
                summary.type = PacketType::Initial;
                summary.srcID = connIDs.pick_up_srcID();
                summary.dstID = connIDs.pick_up_dstID(status.is_server());
                summary.token = retry_token.get_token();
                if (!write_packet(w, summary, mode, waiters)) {
                    return false;
                }
                return true;
            }

            // write_handshake generates handshake packet
            // if succeeded, may drop initial key
            bool write_handshake(io::writer& w, WriteMode mode, ack::ACKWaiters& waiters) {
                packet::PacketSummary summary;
                summary.type = PacketType::Handshake;
                summary.srcID = connIDs.pick_up_srcID();
                summary.dstID = connIDs.pick_up_dstID(status.is_server());
                if (!write_packet(w, summary, mode, waiters)) {
                    return false;
                }
                if (!status.is_server() && crypto.maybe_drop_initial()) {
                    ackh.on_packet_number_space_discarded(status, status::PacketNumberSpace::initial);
                    logger.debug("drop initial keys");
                }
                return true;
            }

            // write_onertt generates 1-RTT packet
            bool write_onertt(io::writer& w, WriteMode mode, ack::ACKWaiters& waiters) {
                packet::PacketSummary summary;
                summary.type = PacketType::OneRTT;
                summary.dstID = connIDs.pick_up_dstID(status.is_server());
                if (!write_packet(w, summary, mode, waiters)) {
                    return false;
                }
                return true;
            }

            bool write_zerortt(io::writer& w, WriteMode mode, ack::ACKWaiters& waiters) {
                if (status.is_server()) {
                    return true;
                }
                packet::PacketSummary summary;
                summary.type = PacketType::ZeroRTT;
                summary.srcID = connIDs.pick_up_srcID();
                summary.dstID = connIDs.pick_up_dstID(status.is_server());
                if (!write_packet(w, summary, mode, waiters)) {
                    return false;
                }
                return true;
            }

            std::pair<view::rvec, bool> create_encrypted_packet(bool on_close) {
                packet_creation_buffer.resize(mtu.current_datagram_size());
                io::writer w{packet_creation_buffer};
                const bool has_initial = crypto.write_installed(PacketType::Initial) &&
                                         status.can_send(status::PacketNumberSpace::initial);
                bool has_handshake = crypto.write_installed(PacketType::Handshake) &&
                                     status.can_send(status::PacketNumberSpace::handshake);
                const bool has_onertt = crypto.write_installed(PacketType::OneRTT) &&
                                        status.can_send(status::PacketNumberSpace::application);
                const auto to_mode = [&](bool b) {
                    if (on_close) {
                        return b ? WriteMode::use_all_buffer | WriteMode::conn_close : WriteMode::conn_close;
                    }
                    else {
                        return b ? WriteMode::use_all_buffer : WriteMode::none;
                    }
                };
                ack::ACKWaiters waiters;
                if (has_initial) {
                    if (!write_initial(w, to_mode(!has_handshake && !has_onertt), waiters)) {
                        return {{}, false};
                    }
                }
                if (!on_close && has_handshake && crypto.maybe_drop_handshake()) {
                    on_handshake_confirmed();
                    has_handshake = false;
                }
                if (has_handshake) {
                    if (!write_handshake(w, to_mode(has_initial && !has_onertt), waiters)) {
                        return {{}, false};
                    }
                }
                if (has_onertt) {
                    if (!write_onertt(w, to_mode(has_initial), waiters)) {
                        return {{}, false};
                    }
                }
                if (!on_close) {
                    logger.loss_timer_state(status.loss_timer(), status.now());
                }
                return {w.written(), true};
            }

            std::pair<view::rvec, bool> create_mtu_probe_packet() {
                const bool has_onertt = crypto.write_installed(PacketType::OneRTT) &&
                                        status.can_send(status::PacketNumberSpace::application);
                if (!has_onertt) {
                    return {{}, true};  // nothing to do
                }
                ack::ACKWaiters waiter;
                auto [size, required] = mtu.probe_required(waiter);
                if (!required) {
                    return {{}, true};  // nothing to do too
                }
                packet_creation_buffer.resize(size);
                io::writer w{packet_creation_buffer};
                if (!write_onertt(w, WriteMode::mtu_probe, waiter)) {
                    return {{}, false};
                }
                auto probe = w.written();
                if (probe.size() != size) {
                    set_quic_runtime_error(QUICError{
                        .msg = "MTU Probe packet creation failure. library bug!!",
                    });
                    return {{}, false};  // fatal error
                }
                logger.mtu_probe(probe.size());
                return {probe, true};
            }

            std::pair<view::rvec, bool> create_connection_close_packet() {
                close_lock.lock();
                const auto d = helper::defer([&] {
                    close_lock.unlock();
                });
                if (!closer.has_error()) {
                    return {{}, true};  // nothing to do
                }
                if (status.close_timeout()) {
                    return {{}, false};  // finish waiting
                }
                if (auto sent = closer.sent_packet(); !sent.null()) {
                    if (!closer.should_retransmit()) {
                        return {{}, true};  // nothing to do
                    }
                    closer.on_close_retransmited();
                    return {sent, true};
                }
                if (closer.close_by_peer()) {
                    return {{}, false};  // close by peer
                }
                auto [packet, ok] = create_encrypted_packet(true);
                if (!ok) {
                    logger.debug("fatal error!! connection close packet creation failure");
                    return {{}, false};  // fatal error!!!!!
                }
                closer.on_close_packet_sent(packet);
                status.set_close_timer();
                closed.store(
                    closer.error_type() == close::ErrorType::app
                        ? close::CloseReason::close_by_local_app
                        : close::CloseReason::close_by_local_runtime);  // local close
                return {packet, true};
            }

            bool set_transport_parameter() {
                // set transport parameter
                packet_creation_buffer.resize(1200);
                io::writer w{packet_creation_buffer};
                if (!params.render_local(w, status.is_server())) {
                    set_quic_runtime_error(error::Error("failed to render params"));
                    return false;
                }
                auto p = w.written();
                auto err = crypto.tls().set_quic_transport_params(p);
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                return true;
            }

            bool conn_timeout() {
                if (status.is_idle_timeout()) {
                    if (status.handshake_status().handshake_confirmed()) {
                        set_quic_runtime_error(err_idle_timeout);
                        logger.debug("connection close: idle timeout");
                        closed.store(close::CloseReason::idle_timeout);  // idle timeout
                        return true;
                    }
                    else {
                        set_quic_runtime_error(err_handshake_timeout);
                        logger.debug("connection close: handshake timeout");
                        closed.store(close::CloseReason::handshake_timeout);  // handshake timeout
                        return true;
                    }
                }
                return false;
            }

            bool maybe_loss_timeout() {
                if (auto err = ackh.maybe_loss_detection_timeout(status, nullptr)) {
                    set_quic_runtime_error(std::move(err));
                    logger.debug("error at loss detection timeout");
                    return false;
                }
                return true;
            }

            bool handle_frame(const packet::PacketSummary& summary, const frame::Frame& frame) {
                if (auto ack = frame::cast<frame::ACKFrame<slib::vector>>(&frame)) {
                    ack::RemovedPackets rem;
                    auto err = ackh.on_ack_received(status, status::from_packet_type(summary.type), rem, nullptr, *ack, [&] {
                        stream::impl::Conn<Lock>* ptr = streams.get();
                        return ptr->is_flow_control_limited();
                    });
                    if (err) {
                        set_quic_runtime_error(std::move(err));
                        return false;
                    }
                    logger.rtt_state(status.rtt_status(), status.now());
                    return true;
                }
                if (frame.type.type_detail() == FrameType::CRYPTO ||
                    frame.type.type_detail() == FrameType::HANDSHAKE_DONE) {
                    auto err = crypto.recv(summary.type, frame);
                    if (err) {
                        set_quic_runtime_error(std::move(err));
                        return false;
                    }
                    if (crypto.handshake_complete()) {
                        status.on_handshake_complete();
                        if (!apply_transport_param(summary)) {
                            return false;
                        }
                    }
                    return true;
                }
                if (is_ConnRelated(frame.type.type_detail()) ||
                    is_BidiStreamOK(frame.type.type_detail())) {
                    stream::impl::Conn<Lock>* ptr = streams.get();
                    error::Error tmp_err;
                    std::tie(std::ignore, tmp_err) = ptr->recv(frame);
                    if (tmp_err) {
                        set_quic_runtime_error(std::move(tmp_err));
                        return false;
                    }
                    return true;
                }
                if (frame.type.type_detail() == FrameType::NEW_CONNECTION_ID ||
                    frame.type.type_detail() == FrameType::RETIRE_CONNECTION_ID) {
                    auto err = connIDs.recv(summary, frame);
                    if (err) {
                        set_quic_runtime_error(std::move(err));
                        return false;
                    }
                    return true;
                }
                if (frame.type.type_detail() == FrameType::PATH_CHALLENGE ||
                    frame.type.type_detail() == FrameType::PATH_RESPONSE) {
                    auto err = path_verifyer.recv(frame);
                    if (err) {
                        set_quic_runtime_error(std::move(err));
                        return false;
                    }
                    return true;
                }
                if (frame.type.type() == FrameType::CONNECTION_CLOSE) {
                    recv_close(static_cast<const frame::ConnectionCloseFrame&>(frame));
                    return true;
                }
                // TODO(on-keyday): now skip implementing
                // DATAGRAN and NEW_TOKEN
                return true;
            }

            bool handle_on_initial(packet::PacketSummary summary) {
                if (summary.type == PacketType::Initial) {
                    if (!connIDs.initial_conn_id_accepted()) {
                        if (auto err = connIDs.on_initial_packet_received(version, summary.srcID)) {
                            set_quic_runtime_error(std::move(err));
                            return false;
                        }
                        if (status.is_server()) {
                            if (!connIDs.local_using_zero_length()) {
                                if (auto id = connIDs.issue(src_connID_len, false); id.seq == -1) {
                                    set_quic_runtime_error(error::Error("failed to generate connection ID"));
                                    return false;
                                }
                            }
                            logger.debug("save client connection ID");
                        }
                        else {
                            logger.debug("save server connection ID");
                        }
                    }
                }
                return true;
            }

            bool check_connID(packet::PacketSummary& prev, packet::PacketSummary sum) {
                if (prev.type != PacketType::Unknown) {
                    if (sum.dstID != prev.dstID) {
                        logger.drop_packet(sum.type, sum.packet_number, error::Error("using not same dstID"));
                        return false;
                    }
                    if (sum.type != PacketType::OneRTT) {
                        if (sum.srcID != prev.srcID) {
                            logger.drop_packet(sum.type, sum.packet_number, error::Error("using not same srcID"));
                            return false;
                        }
                    }
                }
                else {
                    if (!status.is_server()) {  // client
                        if (sum.type != PacketType::OneRTT &&
                            (sum.type != PacketType::Initial ||
                             connIDs.initial_conn_id_accepted())) {
                            if (!connIDs.has_dstID(sum.srcID)) {
                                logger.drop_packet(sum.type, sum.packet_number, error::Error("invalid srcID"));
                                return false;
                            }
                        }
                        if (!connIDs.has_srcID(sum.dstID)) {
                            logger.drop_packet(sum.type, sum.packet_number, error::Error("invalid dstID"));
                            return false;
                        }
                    }
                    else {  // server
                        if (sum.type != PacketType::Initial ||
                            connIDs.initial_conn_id_accepted()) {
                            if (sum.type != PacketType::OneRTT) {
                                if (!connIDs.has_dstID(sum.srcID)) {
                                    logger.drop_packet(sum.type, sum.packet_number, error::Error("invalid srcID"));
                                    return false;
                                }
                            }
                            if (!connIDs.has_srcID(sum.dstID)) {
                                logger.drop_packet(sum.type, sum.packet_number, error::Error("invalid dstID"));
                                return false;
                            }
                        }
                    }
                }
                prev = sum;
                return true;
            }

            bool handle_single_packet(packet::PacketSummary summary, view::rvec payload, std::uint64_t length) {
                auto pn_space = status::from_packet_type(summary.type);
                if (summary.type == PacketType::OneRTT) {
                    summary.version = version;
                }
                status.on_packet_decrypted(pn_space);
                logger.recv_packet(summary, payload);
                if (!handle_on_initial(summary)) {
                    return false;
                }
                io::reader r{payload};
                packet::PacketStatus pstatus;
                if (!frame::parse_frames<slib::vector>(r, [&](frame::Frame& f, bool err) {
                        if (err) {
                            set_quic_runtime_error(QUICError{
                                .msg = "frame encoding error",
                                .transport_error = TransportError::FRAME_ENCODING_ERROR,
                                .frame_type = f.type.type_detail(),
                            });
                            return false;
                        }
                        if (!is_OKFrameForPacket(summary.type, f.type.type_detail())) {
                            set_quic_runtime_error(QUICError{
                                .msg = not_allowed_frame_message(summary.type),
                                .transport_error = TransportError::PROTOCOL_VIOLATION,
                                .packet_type = summary.type,
                                .frame_type = f.type.type_detail(),
                            });
                            return false;
                        }
                        pstatus.apply(f.type.type_detail());
                        if (!handle_frame(summary, f)) {
                            return false;
                        }
                        return true;
                    })) {
                    return false;
                }
                if (summary.type == PacketType::OneRTT && crypto.maybe_drop_handshake()) {
                    on_handshake_confirmed();
                }
                unacked.on_packet_processed(pn_space, summary.packet_number, pstatus.is_ack_eliciting);
                status.on_packet_processed(pn_space, summary.packet_number);
                logger.loss_timer_state(status.loss_timer(), status.now());
                return true;
            }

            bool handle_retry_packet(packet::RetryPacket retry) {
                if (status.is_server()) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("received Retry packet at server"));
                    return true;
                }
                if (status.handshake_status().has_received_packet()) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("unexpected Retry packet after receiveing packet"));
                    return true;
                }
                if (status.has_received_retry()) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("unexpected Retry packet after receiveing Retry"));
                    return true;
                }
                packet::RetryPseduoPacket pseduo;
                pseduo.from_retry_packet(connIDs.get_initial(), retry);
                packet_creation_buffer.resize(pseduo.len());
                io::writer w{packet_creation_buffer};
                if (!pseduo.render(w)) {
                    set_quic_runtime_error(error::Error("failed to render Retry packet. library bug!!"));
                    return false;
                }
                byte output[16];
                auto err = crypto::generate_retry_integrity_tag(output, w.written(), version);
                if (err) {
                    set_quic_runtime_error(err);
                    return false;
                }
                if (view::rvec(retry.retry_integrity_tag) != output) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("retry integrity tags are not matched. maybe observer exists or packet broken"));
                    return true;
                }
                ackh.on_retry_received(status);
                connIDs.on_retry_received(retry.srcID);
                retry_token.on_retry_received(retry.retry_token);
                crypto.install_initial_secret(version, retry.srcID);
                return true;
            }

           public:
            // thread safe call
            bool is_closed() const {
                return closed != close::CloseReason::not_closed;
            }

            // thread safe call
            // if err is QUICError, use detail of it
            // otherwise wrap error
            void request_close(error::Error err) {
                close_lock.lock();
                const auto d = helper::defer([&] {
                    close_lock.unlock();
                });
                if (closer.has_error()) {
                    return;  // already set
                }
                auto qerr = err.as<QUICError>();
                if (qerr) {
                    qerr->is_app = true;
                    qerr->by_peer = false;
                }
                else {
                    auto q = QUICError{
                        .msg = "application layer error",
                        .transport_error = TransportError::APPLICATION_ERROR,
                        .base = std::move(err),
                        .is_app = true,
                    };
                    err = std::move(q);
                }
                closer.on_error(std::move(err), close::ErrorType::app);
            }

            const error::Error& get_conn_error() const {
                return closer.get_error();
            }

            auto get_streams() const {
                return streams;
            }

            bool init(Config&& config) {
                closer.reset();
                error::Error tmp_err;
                tls::TLS tls;
                std::tie(tls, tmp_err) = tls::create_quic_tls_with_error(config.tls_config, crypto::qtls_callback, &crypto);
                if (tmp_err) {
                    set_quic_runtime_error(std::move(tmp_err));
                    return false;
                }
                version = config.version;
                crypto.reset(std::move(tls), config.server);
                streams = stream::impl::make_conn<Lock>(config.server
                                                            ? stream::Direction::server
                                                            : stream::Direction::client);
                status::InternalConfig internal;
                static_cast<status::Config&>(internal) = config.internal_parameters;
                internal.ack_delay_exponent = config.transport_parameters.ack_delay_exponent;
                internal.idle_timeout = config.transport_parameters.max_idle_timeout;
                mtu.reset(config.path_parameters);
                status.reset(internal, {}, config.server, mtu.current_datagram_size());
                params.local = std::move(config.transport_parameters);
                params.local_box.boxing(params.local);
                src_connID_len = config.connIDLen;
                connIDs.reset(config.connIDLen == 0, std::move(config.random));
                logger = std::move(config.logger);
                retry_token.reset();
                if (!status.now().valid()) {
                    set_quic_runtime_error(QUICError{
                        .msg = "clock.now() returns invalid time. clock must be set",
                    });
                    return false;
                }
                closed.store(close::CloseReason::not_closed);
                return true;
            }

            bool connect_start() {
                if (status.handshake_started()) {
                    return false;
                }
                if (status.is_server()) {
                    return false;
                }
                status.on_handshake_start();
                if (src_connID_len != 0) {
                    auto issued = connIDs.issue(src_connID_len, false);
                    if (issued.seq != 0) {
                        set_quic_runtime_error(error::Error("failed to issue connection ID with sequence number 0. library bug!!"));
                        return false;
                    }
                    params.set_local(trsparam::make_transport_param(trsparam::DefinedID::initial_src_connection_id, issued.id));
                }
                else {
                    params.set_local(trsparam::make_transport_param(trsparam::DefinedID::initial_src_connection_id, {}));
                }

                if (!connIDs.gen_initial_random(src_connID_len)) {
                    set_quic_runtime_error(error::Error("failed to generate client destination connection ID. library bug!!"));
                    return false;
                }
                if (!crypto.install_initial_secret(version, connIDs.pick_up_dstID(false))) {
                    return false;
                }
                if (!set_transport_parameter()) {
                    return false;
                }
                auto err = crypto.tls().connect();
                if (!tls::isTLSBlock(err)) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                return true;
            }

            bool accept_start() {
                if (status.handshake_started()) {
                    return false;
                }
                if (!status.is_server()) {
                    return false;
                }
                status.on_handshake_start();
                auto err = crypto.tls().accept();
                if (!tls::isTLSBlock(err)) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                return true;
            }

            // returns (payload,idle)
            std::pair<view::rvec, bool> create_udp_payload() {
                if (closer.has_error()) {
                    return create_connection_close_packet();
                }
                if (conn_timeout()) {
                    return {{}, false};
                }
                if (!maybe_loss_timeout()) {
                    // error on loss detection
                    return create_connection_close_packet();
                }
                auto [packet, ok] = create_encrypted_packet(false);
                if (!ok) {
                    return create_connection_close_packet();
                }
                if (packet.size()) {
                    return {packet, ok};
                }
                std::tie(packet, ok) = create_mtu_probe_packet();
                if (!ok) {
                    return create_connection_close_packet();
                }
                return {packet, true};
            }

            bool parse_udp_payload(view::wvec data) {
                if (!closer.sent_packet().null()) {
                    closer.on_recv_after_close_sent();
                    return true;  // ignore
                }
                auto is_stateless_reset = connIDs.get_stateless_reset_callback();
                auto get_dstID_len = connIDs.get_dstID_len_callback();
                packet::PacketSummary prev;
                prev.type = PacketType::Unknown;
                auto crypto_cb = [&](packetnum::Value pn, auto&& packet, view::wvec raw_packet) {
                    return handle_single_packet(packet::summarize(packet, pn), packet.payload, raw_packet.size());
                };
                auto check_connID_cb = [&](auto&& packet) {
                    packet::PacketSummary sum = packet::summarize(packet, packetnum::infinity);
                    bool first_time = prev.type == PacketType::Unknown;
                    if (!check_connID(prev, sum)) {
                        return false;
                    }
                    if (first_time) {
                        status.on_datagram_received(data.size());
                    }
                    return true;
                };
                auto decrypt_suite = crypto.decrypt_callback(version, [&](PacketType type) {
                    return status.largest_received_packet_number(status::from_packet_type(type));
                });
                auto plain_cb = [this](PacketType, auto&& packet, view::wvec src) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(packet)>, packet::RetryPacket>) {
                        return handle_retry_packet(packet);
                    }
                    return true;
                };
                auto decrypt_err = [&](PacketType type, auto&& packet, packet::CryptoPacket cp, error::Error err, bool is_decrypted) {
                    logger.drop_packet(type, cp.packet_number, std::move(err));
                    // decrypt failure doesn't make packet parse failure
                    // see https://tex2e.github.io/rfc-translater/html/rfc9000#12-2--Coalescing-Packets
                    return true;
                };
                auto plain_err = [&](PacketType type, auto&& packet, view::wvec src, bool err, bool valid_type) {
                    logger.drop_packet(type, packetnum::infinity, error::Error("plain packet failure"));
                    return false;
                };
                return crypto::parse_with_decrypt<slib::vector>(
                    data, crypto::authentication_tag_length, is_stateless_reset, get_dstID_len,
                    crypto_cb, decrypt_suite, plain_cb, decrypt_err, plain_err, check_connID_cb);
            }
        };
    }  // namespace fnet::quic::context
}  // namespace utils

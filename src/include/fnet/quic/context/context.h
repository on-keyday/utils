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
#include "config.h"

namespace utils {
    namespace fnet::quic::context {

        enum class WriteMode {
            contain_initial,
            write_as_we_can,
            mtu_probe,
        };

        constexpr auto err_no_payload = error::Error("NO_PAYLOAD");
        constexpr auto err_creation = error::Error("CREATION");
        constexpr auto err_idle_timeout = error::Error("IDLE_TIMEOUT");
        constexpr auto err_handshake_timeout = error::Error("HANDSHAKE_TIMEOUT");

        constexpr bool is_timeout(const error::Error& err) {
            return err == err_idle_timeout || err == err_handshake_timeout;
        }

        template <class Lock>
        struct Context : std::enable_shared_from_this<Context<Lock>> {
            std::uint32_t version = 0;
            status::Status<status::NewReno> status;
            trsparam::TransportParameters params;
            crypto::CryptoSuite crypto;
            ack::SentPacketHistory ackh;
            ack::UnackedPackets unacked;
            connid::IDManager connIDs;
            token::RetryToken retry_token;
            path::MTU mtu;
            path::PathVerifier<slib::deque> path_verifyer;
            std::shared_ptr<stream::impl::Conn<Lock>> streams;

            log::Logger logger;

            // internal buffer
            flex_storage packet_creation_buffer;
            error::Error conn_err;
            bool transport_param_read = false;

            bool apply_transport_param(packet::PacketSummary summary) {
                if (transport_param_read) {
                    return true;
                }
                auto data = crypto.tls.get_peer_quic_transport_params();
                if (data.null()) {
                    return true;  // nothing can do
                }
                io::reader r{data};
                auto err = params.parse_peer(r, status.is_server());
                if (err) {
                    conn_err = std::move(err);
                    return false;
                }
                auto& peer = params.peer;
                // get registered initial_source_connection_id by peer
                auto connID = connIDs.acceptor.choose(0);
                if (peer.initial_src_connection_id.null() || peer.initial_src_connection_id != connID.id) {
                    conn_err = QUICError{
                        .msg = "initial_src_connection_id is not matched to id contained on initial packet",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                    };
                    return false;
                }
                if (!status.is_server()) {  // client
                    if (peer.original_dst_connection_id != connIDs.initial_random) {
                        conn_err = QUICError{
                            .msg = "original_dst_connection_id is not matched to id contained on initial packet",
                            .transport_error = TransportError::PROTOCOL_VIOLATION,
                        };
                        return false;
                    }
                    if (status.has_received_retry()) {
                        if (peer.retry_src_connection_id != connIDs.retry_random) {
                            conn_err = QUICError{
                                .msg = "retry_src_connection_id is not matched to id previous received",
                                .transport_error = TransportError::PROTOCOL_VIOLATION,
                            };
                            return false;
                        }
                    }
                    connIDs.on_initial_stateless_reset_token_received(peer.stateless_reset_token);
                    if (!peer.preferred_address.connectionID.null()) {
                        connIDs.on_preferred_address_received(peer.preferred_address.connectionID, peer.preferred_address.stateless_reset_token);
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
                    conn_err = QUICError{
                        .msg = to_string(res),
                        .frame_type = fw.prev_type,
                    };
                    return false;
                };
                if (summary.type != PacketType::ZeroRTT) {
                    if (auto err = unacked.send(fw, pn_space)) {
                        conn_err = std::move(err);
                        return false;
                    }
                }
                if (status.is_probe_required(pn_space) || status.should_send_any_packet()) {
                    logger.pto_fire(pn_space);
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
                        // least 4 byte needed for sample skip size
                        // see https://datatracker.ietf.org/doc/html/rfc9001#section-5.4.2
                        if (w.written().size() + wire.len < crypto::sample_skip_size) {
                            fw.write(frame::PaddingFrame{});  // apply one
                            w.write(0, crypto::sample_skip_size - w.written().size() - wire.len);
                        }
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
                conn_err = ackh.on_packet_sent(status, pn_space, std::move(sent));
                if (conn_err) {
                    ack::mark_as_lost(sent.waiters);
                    return false;
                }
                return true;
            }

            bool write_packet(io::writer& w, packet::PacketSummary summary, WriteMode mode, ack::ACKWaiters& waiters) {
                // get packet number space
                auto pn_space = status::from_packet_type(summary.type);
                if (pn_space == status::PacketNumberSpace::no_space) {
                    conn_err = error::Error("invalid packet type");
                    return false;
                }

                // set QUIC version and packet number
                const auto [pn, largest_acked] = status.next_and_largest_acked_packet_number(pn_space);
                summary.version = version;
                summary.packet_number = pn;

                // get encryption suite
                crypto::EncryptSuite suite;
                std::tie(suite, conn_err) = crypto.encrypt_suite(version, summary.type);
                if (conn_err) {
                    return false;
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

                if (mode == WriteMode::mtu_probe) {
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
                    const auto fill_all = mode == WriteMode::contain_initial;
                    std::tie(plain, res) = packet::create_packet(
                        copy, summary, largest_acked,
                        crypto::authentication_tag_length, fill_all,
                        render_payload_callback(summary, waiters, no_payload, pstatus, fill_all));
                }

                if (conn_err) {
                    return false;
                }
                if (no_payload) {
                    return true;  // skip. no payload exists
                }
                if (!res) {
                    conn_err = err_creation;
                    return false;
                }

                // encrypt packet
                conn_err = crypto::encrypt_packet(*suite.keys, *suite.cipher, plain);
                if (conn_err) {
                    return false;
                }

                mark_lost.cancel();

                // save packet
                if (!save_sent_packet(summary, pstatus, plain.src.size(), waiters, mode == WriteMode::mtu_probe)) {
                    return false;
                }

                status.consume_packet_number(pn_space);
                w = copy;  // restore

                return true;
            }

            std::pair<view::rvec, bool> create_encrypted_packet() {
                packet_creation_buffer.resize(mtu.current_datagram_size());
                io::writer w{packet_creation_buffer};
                const bool has_initial = crypto.write_installed(PacketType::Initial) &&
                                         status.can_send(status::PacketNumberSpace::initial);
                bool has_handshake = crypto.write_installed(PacketType::Handshake) &&
                                     status.can_send(status::PacketNumberSpace::handshake);
                const bool has_onertt = crypto.write_installed(PacketType::OneRTT) &&
                                        status.can_send(status::PacketNumberSpace::application);
                constexpr auto to_mode = [](bool b) {
                    return b ? WriteMode::contain_initial : WriteMode::write_as_we_can;
                };
                ack::ACKWaiters waiters;
                if (has_initial) {
                    packet::PacketSummary summary;
                    summary.type = PacketType::Initial;
                    summary.srcID = connIDs.issuer.pick_up_id();
                    summary.dstID = connIDs.acceptor.pick_up_id();
                    if (summary.dstID.null()) {
                        if (status.has_received_retry()) {
                            summary.dstID = connIDs.retry_random;
                            summary.token = retry_token.get_token();
                        }
                        else {
                            summary.dstID = connIDs.initial_random;
                        }
                    }
                    if (!write_packet(w, summary, to_mode(!has_handshake && !has_onertt), waiters)) {
                        return {{}, false};
                    }
                }
                if (has_handshake && crypto.maybe_drop_handshake()) {
                    on_handshake_confirmed();
                    has_handshake = false;
                }
                if (has_handshake) {
                    packet::PacketSummary summary;
                    summary.type = PacketType::Handshake;
                    summary.srcID = connIDs.issuer.pick_up_id();
                    summary.dstID = connIDs.acceptor.pick_up_id();
                    if (!write_packet(w, summary, to_mode(has_initial && !has_onertt), waiters)) {
                        return {{}, false};
                    }
                    if (!status.is_server() && crypto.maybe_drop_initial()) {
                        ackh.on_packet_number_space_discarded(status, status::PacketNumberSpace::initial);
                        logger.debug("drop initial keys");
                    }
                }
                if (has_onertt) {
                    packet::PacketSummary summary;
                    summary.type = PacketType::OneRTT;
                    summary.dstID = connIDs.acceptor.pick_up_id();
                    if (!write_packet(w, summary, to_mode(has_initial), waiters)) {
                        return {{}, false};
                    }
                }
                logger.loss_timer_state(status.loss_timer(), status.now());
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
                packet::PacketSummary summary;
                summary.type = PacketType::OneRTT;
                summary.dstID = connIDs.acceptor.pick_up_id();
                if (!write_packet(w, summary, WriteMode::mtu_probe, waiter)) {
                    return {{}, false};  // fatal error
                }
                auto probe = w.written();
                if (probe.size() != size) {
                    conn_err = QUICError{
                        .msg = "MTU Probe packet creation failure. library bug!!",
                    };
                    return {{}, false};  // fatal error
                }
                return {probe, true};
            }

            // returns (payload,has_err)
            std::pair<view::rvec, bool> create_udp_payload() {
                if (!detect_timeout()) {
                    return {{}, false};
                }
                auto [packet, ok] = create_encrypted_packet();
                if (!ok || packet.size()) {
                    return {packet, ok};
                }
                std::tie(packet, ok) = create_mtu_probe_packet();
                if (!ok || packet.size()) {
                    return {packet, ok};
                }
                return {{}, true};  // nothing to send
            }

            bool init(Config&& config) {
                std::tie(crypto.tls, conn_err) = tls::create_quic_tls_with_error(config.tls_config, crypto::qtls_callback, &crypto);
                if (conn_err) {
                    return false;
                }
                version = config.version;
                crypto.is_server = config.server;
                streams = stream::impl::make_conn<Lock>(config.server
                                                            ? stream::Direction::server
                                                            : stream::Direction::client);
                status::InternalConfig internal;
                static_cast<status::Config&>(internal) = config.internal_parameters;
                internal.ack_delay_exponent = config.transport_parameters.ack_delay_exponent;
                internal.idle_timeout = config.transport_parameters.max_idle_timeout;
                status.reset(internal, {}, config.server, mtu.current_datagram_size());
                params.local = std::move(config.transport_parameters);
                params.local_box.boxing(params.local);
                connIDs.issuer.random = std::move(config.random);
                mtu.reset(config.path_parameters);
                logger = std::move(config.logger);
                if (!status.now().valid()) {
                    conn_err = QUICError{
                        .msg = "clock.now() returns invalid time. clock must be set",
                    };
                    return false;
                }
                return true;
            }

            bool set_transport_parameter() {
                // set transport parameter
                packet_creation_buffer.resize(1200);
                io::writer w{packet_creation_buffer};
                if (!params.render_local(w, status.is_server())) {
                    conn_err = error::Error("failed to render params");
                    return false;
                }
                auto p = w.written();
                conn_err = crypto.tls.set_quic_transport_params(p);
                if (conn_err) {
                    return false;
                }
                return true;
            }

            bool connect_start() {
                if (status.is_server()) {
                    return false;
                }
                if (auto issued = connIDs.issuer.issue(10, true); issued.seq == -1) {
                    return false;
                }
                else {
                    params.set_local(trsparam::make_transport_param(trsparam::DefinedID::initial_src_connection_id, issued.id));
                }
                if (connIDs.gen_initial_random(10).null()) {
                    return false;
                }
                if (!crypto.install_initial_secret(version, connIDs.initial_random)) {
                    return false;
                }
                if (!set_transport_parameter()) {
                    return false;
                }
                auto err = crypto.tls.connect();
                if (!tls::isTLSBlock(err)) {
                    conn_err = std::move(err);
                    return false;
                }
                return true;
            }

            bool detect_timeout() {
                if (status.is_idle_timeout()) {
                    if (status.handshake_status().handshake_confirmed()) {
                        conn_err = err_idle_timeout;
                        logger.debug("connection close: idle timeout");
                        return false;
                    }
                    else {
                        conn_err = err_handshake_timeout;
                        logger.debug("connection close: handshake timeout");
                        return false;
                    }
                }
                if (auto err = ackh.maybe_loss_detection_timeout(status, nullptr)) {
                    conn_err = std::move(err);
                    logger.debug("error at loss detection timeout");
                    return false;
                }
                return true;
            }

            bool handle_frame(const packet::PacketSummary& summary, const frame::Frame& frame) {
                if (auto ack = frame::cast<frame::ACKFrame<slib::vector>>(&frame)) {
                    ack::RemovedPackets rem;
                    conn_err = ackh.on_ack_received(status, status::from_packet_type(summary.type), rem, nullptr, *ack, [&] {
                        stream::impl::Conn<Lock>* ptr = streams.get();
                        return ptr->is_flow_control_limited();
                    });
                    return conn_err.is_noerr();
                }
                if (frame.type.type_detail() == FrameType::CRYPTO ||
                    frame.type.type_detail() == FrameType::HANDSHAKE_DONE) {
                    conn_err = crypto.recv(summary.type, frame);
                    if (conn_err) {
                        return false;
                    }
                    if (crypto.handshake_complete) {
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
                    std::tie(std::ignore, conn_err) = ptr->recv(frame);
                    return conn_err.is_noerr();
                }
                if (frame.type.type_detail() == FrameType::NEW_CONNECTION_ID ||
                    frame.type.type_detail() == FrameType::RETIRE_CONNECTION_ID) {
                    conn_err = connIDs.recv(frame);
                    return conn_err.is_noerr();
                }
                if (frame.type.type_detail() == FrameType::PATH_CHALLENGE ||
                    frame.type.type_detail() == FrameType::PATH_RESPONSE) {
                    conn_err = path_verifyer.recv(frame);
                    return conn_err.is_noerr();
                }
                if (frame.type.type() == FrameType::CONNECTION_CLOSE) {
                }
                // TODO(on-keyday): now skip implementing
                // DATAGRAN and NEW_TOKEN
                return true;
            }

            bool handle_single_packet(packet::PacketSummary summary, view::rvec payload, std::uint64_t length) {
                auto pn_space = status::from_packet_type(summary.type);
                status.on_packet_received(pn_space, length);
                logger.recv_packet(summary, payload);
                if (summary.type == PacketType::Initial) {
                    if (summary.srcID.size() == 0) {
                        connIDs.acceptor.use_zero_length = true;
                    }
                    else {
                        if (connIDs.on_initial_packet_received(summary.srcID)) {
                            logger.debug("save server connection ID");
                        }
                    }
                }
                io::reader r{payload};
                packet::PacketStatus pstatus;
                if (!frame::parse_frames<slib::vector>(r, [&](frame::Frame& f, bool err) {
                        if (err) {
                            conn_err = QUICError{
                                .msg = "frame encoding error",
                                .transport_error = TransportError::FRAME_ENCODING_ERROR,
                                .frame_type = f.type.type_detail(),
                            };
                            return false;
                        }
                        if (!is_OKFrameForPacket(summary.type, f.type.type_detail())) {
                            conn_err = QUICError{
                                .msg = not_allowed_frame_message(summary.type),
                                .transport_error = TransportError::PROTOCOL_VIOLATION,
                                .packet_type = summary.type,
                                .frame_type = f.type.type_detail(),
                            };
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
                    conn_err = QUICError{
                        .msg = "received Retry packet at server",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                    };
                    return false;
                }
                if (status.handshake_status().handshake_packet_is_received()) {
                    conn_err = QUICError{
                        .msg = "unexpected Retry packet after Handshake key installed",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                    };
                    return false;
                }
                packet::RetryPseduoPacket pseduo;
                pseduo.from_retry_packet(connIDs.initial_random, retry);
                packet_creation_buffer.resize(pseduo.len());
                io::writer w{packet_creation_buffer};
                if (!pseduo.render(w)) {
                    conn_err = error::Error("failed to render Retry packet. library bug!!");
                    return false;
                }
                byte output[16];
                conn_err = crypto::generate_retry_integrity_tag(output, w.written(), version);
                if (conn_err) {
                    return false;
                }
                if (view::rvec(retry.retry_integrity_tag) != output) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("retry integrity tags are not matched. maybe observer exists or packet broken"));
                    return true;
                }
                status.on_retry_received();
                connIDs.on_retry_received(retry.srcID);
                retry_token.on_retry_received(retry.retry_token);
                return true;
            }

            bool parse_udp_payload(view::wvec data) {
                auto is_stateless_reset = [](packet::StatelessReset) { return false; };
                auto get_dstID_len = connIDs.get_dstID_len_callback();
                auto crypto_cb = [&](packetnum::Value pn, auto&& packet, view::wvec raw_packet) {
                    return handle_single_packet(packet::summarize(packet, pn), packet.payload, raw_packet.size());
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
                    crypto_cb, decrypt_suite, plain_cb, decrypt_err, plain_err);
            }
        };
    }  // namespace fnet::quic::context
}  // namespace utils

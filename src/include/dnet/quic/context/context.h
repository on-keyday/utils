/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// context - quic context suite
#pragma once
#include "../crypto/suite.h"
#include "../ack/loss_detection.h"
#include "../packet/creation.h"
#include "../conn/id_manage.h"
#include "../event/event.h"
#include "../event/crypto_event.h"
#include "../mtu/mtu.h"
#include "../crypto/crypto_tag.h"
#include "../crypto/crypto.h"
#include "../transport_parameter/suite.h"
#include "../crypto/callback.h"
#include "../crypto/decrypt.h"
#include "../frame/parse.h"
#include "../event/ack_event.h"
#include "../event/conn_id_event.h"
#include "../stream/impl/conn_manage.h"
#include "../log/log.h"

namespace utils {
    namespace dnet::quic::context {

        constexpr auto err_no_payload = error::Error("NO_PAYLOAD");
        constexpr auto err_creation = error::Error("CREATION");

        struct MinConfig {
            std::uint32_t version = 0;
            bool is_server = false;
        };

        template <class Lock>
        struct Context : std::enable_shared_from_this<Context<Lock>> {
            MinConfig conf;
            crypto::CryptoSuite crypto;
            ack::LossDetectionHandler ackh;
            ack::UnackedPacket unacked;
            conn::IDManager connIDs;
            mtu::PathMTU mtu;
            error::Error conn_err;
            event::EventList list;
            std::shared_ptr<stream::impl::Conn<Lock>> streams;
            log::Logger logger;

            flex_storage packet_creation_buffer;
            trsparam::TransportParameters params;
            bool transport_param_read = false;

            bool apply_transport_param(packet::PacketSummary summary) {
                if (transport_param_read) {
                    return true;
                }
                size_t len = 0;
                auto data = crypto.tls.get_peer_quic_transport_params(&len);
                if (!data) {
                    return true;
                }
                io::reader r{view::rvec(data, len)};
                auto err = params.parse_peer(r, conf.is_server);
                if (err) {
                    conn_err = std::move(err);
                    return false;
                }
                auto& peer = params.peer;
                connIDs.accept_initial(summary.srcID, peer.stateless_reset_token);
                if (!peer.preferred_address.connectionID.null()) {
                    connIDs.accept_transport_param(peer.preferred_address.connectionID, peer.preferred_address.stateless_reset_token);
                }
                mtu.apply_transport_param(peer.max_udp_payload_size);
                auto& local = params.local;
                ackh.apply_transport_param(peer.max_idle_timeout, local.max_idle_timeout);
                auto by_peer = trsparam::to_initial_limits(peer);
                auto by_local = trsparam::to_initial_limits(local);
                streams->apply_initial_limits(by_local, by_peer);
                transport_param_read = true;
                return true;
            }

            auto init_tls() {
                return crypto.tls.make_quic(crypto::qtls_callback, &crypto);
            }

            error::Error write_packet(io::writer& w, packet::PacketSummary summary, bool fill_all) {
                auto pn_space = ack::from_packet_type(summary.type);
                if (pn_space == ack::PacketNumberSpace::no_space) {
                    return error::Error("invalid packet type");
                }
                error::Error err;
                crypto::EncryptSuite suite;
                std::tie(suite, err) = crypto.encrypt_suite(conf.version, summary.type);
                if (err) {
                    return err;
                }
                auto [pn, largest_acked] = ackh.next_packet_number(pn_space);
                summary.packet_number = pn;
                summary.version = conf.version;
                bool no_payload = false;
                event::ACKWaitVec wait;
                packet::PacketStatus status;
                auto render_payload = [&](io::writer& w) {
                    auto offset = w.offset();
                    frame::fwriter fw{w};
                    err = list.send(summary, wait, fw);
                    if (err) {
                        return false;
                    }
                    if (ackh.pto.is_probe_required(pn_space) && !w.full()) {
                        fw.write(frame::PingFrame{});
                    }
                    if (offset >= w.offset()) {
                        no_payload = true;
                        return false;
                    }
                    if (fill_all) {
                        if (!w.full()) {
                            fw.write(frame::PaddingFrame{});  // apply one
                        }
                    }
                    status = fw.status;
                    return true;
                };
                auto copy = w;
                bool initial_used = false;
                auto [plain, res] = packet::create_packet(
                    copy, summary, largest_acked,
                    crypto::authentication_tag_length, fill_all, render_payload);
                if (err) {
                    ack::mark_as_lost(wait);
                    return err;
                }
                if (no_payload) {
                    return err_no_payload;
                }
                if (!res) {
                    ack::mark_as_lost(wait);
                    return err_creation;
                }
                if (auto err = crypto::encrypt_packet(*suite.keys, *suite.cipher, plain)) {
                    ack::mark_as_lost(wait);
                    return err;
                }
                ack::SentPacket sent;
                sent.packet_number = pn;
                sent.type = summary.type;
                sent.ack_eliciting = status.is_ack_eliciting;
                sent.in_flight = status.is_byte_counted;
                sent.sent_bytes = plain.src.size();
                sent.waiters = std::move(wait);
                if (auto err = ackh.on_packet_sent(pn_space, std::move(sent))) {
                    ack::mark_as_lost(sent.waiters);
                    return err;
                }
                ackh.consume_packet_number(pn_space);
                w = copy;
                return error::none;
            }

            bool init_write_events() {
                event::FrameWriteEvent w;
                w.on_event = event::send_crypto;
                w.arg = std::shared_ptr<crypto::CryptoSuite>(this->shared_from_this(), &crypto);
                w.prio = event::priority::crypto;
                list.on_write.push_back(std::move(w));
                w.on_event = event::send_ack;
                w.arg = std::shared_ptr<ack::UnackedPacket>(this->shared_from_this(), &unacked);
                w.prio = event::priority::ack;
                list.on_write.push_back(std::move(w));
                w.on_event = event::send_conn_id;
                w.arg = std::shared_ptr<conn::IDManager>(this->shared_from_this(), &connIDs);
                w.prio = event::priority::conn_id;
                list.on_write.push_back(std::move(w));
                return true;
            }

            bool init_recv_events() {
                event::FrameRecvEvent r;
                r.on_event = event::recv_ack;
                r.arg = std::shared_ptr<ack::LossDetectionHandler>(this->shared_from_this(), &ackh);
                list.on_recv.push_back(std::move(r));
                r.on_event = event::recv_crypto;
                r.arg = std::shared_ptr<crypto::CryptoSuite>(this->shared_from_this(), &crypto);
                list.on_recv.push_back(std::move(r));
                r.on_event = event::recv_conn_id;
                r.arg = std::shared_ptr<conn::IDManager>(this->shared_from_this(), &connIDs);
                list.on_recv.push_back(std::move(r));
                return true;
            }

            error::Error set_tls_config() {
                // set transport parameter
                packet_creation_buffer.resize(1200);
                io::writer w{packet_creation_buffer};
                if (!params.render_local(w, conf.is_server)) {
                    return error::Error("failed to render params");
                }
                auto p = w.written();
                if (auto err = crypto.tls.set_quic_transport_params(p.data(), p.size())) {
                    return err;
                }
                return error::none;
            }

            bool init(bool is_server) {
                conf.is_server = is_server;
                crypto.is_server = is_server;
                ackh.flags.is_server = is_server;
                streams = stream::impl::make_conn<Lock>(is_server
                                                            ? stream::Direction::server
                                                            : stream::Direction::client);
                if (!init_write_events() || !init_recv_events()) {
                    return false;
                }
                return true;
            }

            bool connect_start() {
                if (!init(false)) {
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
                if (!crypto.install_initial_secret(conf.version, connIDs.initial_random)) {
                    return false;
                }
                if (auto err = set_tls_config()) {
                    conn_err = std::move(err);
                    return false;
                }
                auto err = crypto.tls.connect();
                if (!isTLSBlock(err)) {
                    conn_err = std::move(err);
                    return false;
                }
                return true;
            }

            std::pair<view::rvec, bool> create_udp_payload() {
                if (auto err = ackh.maybeTimeout()) {
                    conn_err = std::move(err);
                    return {{}, false};
                }
                packet_creation_buffer.resize(mtu.current_datagram_size());
                io::writer w{packet_creation_buffer};
                const bool has_initial = crypto.write_installed(PacketType::Initial);
                const bool has_handshake = crypto.write_installed(PacketType::Handshake);
                const bool has_onertt = crypto.write_installed(PacketType::OneRTT);
                if (has_initial) {
                    packet::PacketSummary summary;
                    summary.type = PacketType::Initial;
                    summary.srcID = connIDs.issuer.pick_up_id();
                    summary.dstID = connIDs.acceptor.pick_up_id();
                    if (summary.dstID.null()) {
                        summary.dstID = connIDs.initial_random;
                    }
                    if (auto err = write_packet(w, summary, !has_handshake && !has_onertt)) {
                        if (err != err_no_payload) {
                            conn_err = std::move(err);
                            return {{}, false};
                        }
                    }
                }
                if (has_handshake) {
                    if (crypto.maybe_drop_handshake()) {
                        if (auto err = ackh.on_packet_number_space_discarded(ack::PacketNumberSpace::handshake)) {
                            conn_err = std::move(err);
                        }
                        goto ONE_RTT;
                    }
                    packet::PacketSummary summary;
                    summary.type = PacketType::Handshake;
                    summary.srcID = connIDs.issuer.pick_up_id();
                    summary.dstID = connIDs.acceptor.pick_up_id();
                    if (auto err = write_packet(w, summary, has_initial && !has_onertt)) {
                        if (err != err_no_payload) {
                            conn_err = std::move(err);
                            return {{}, false};
                        }
                    }
                    else {
                        if (!conf.is_server && crypto.maybe_drop_initial()) {
                            if (auto err = ackh.on_packet_number_space_discarded(ack::PacketNumberSpace::initial)) {
                                conn_err = std::move(err);
                                return {{}, false};
                            }
                        }
                    }
                }
            ONE_RTT:
                if (has_onertt) {
                    packet::PacketSummary summary;
                    summary.type = PacketType::OneRTT;
                    summary.srcID = connIDs.issuer.pick_up_id();
                    summary.dstID = connIDs.acceptor.pick_up_id();
                    if (auto err = write_packet(w, summary, has_initial)) {
                        if (err != err_no_payload) {
                            conn_err = std::move(err);
                            return {{}, false};
                        }
                    }
                }
                return {w.written(), true};
            }

            bool handle_single_packet(packet::PacketSummary summary, view::rvec payload, std::uint64_t length) {
                if (auto err = ackh.on_packet_recived(ack::from_packet_type(summary.type), length)) {
                    conn_err = std::move(err);
                    return false;
                }
                io::reader r{payload};
                packet::PacketStatus status;
                if (!frame::parse_frames<slib::vector>(r, [&](frame::Frame& f, bool err) {
                        if (err) {
                            conn_err = QUICError{
                                .msg = "frame encoding error",
                                .transport_error = TransportError::FRAME_ENCODING_ERROR,
                                .frame_type = f.type.type_detail(),
                            };
                            return false;
                        }
                        status.apply(f.type.type_detail());
                        if (auto err = list.recv(summary, f)) {
                            conn_err = std::move(err);
                            return false;
                        }
                        if (f.type.type_detail() == FrameType::CRYPTO) {
                            if (!apply_transport_param(summary)) {
                                return false;
                            }
                        }
                        if (f.type.type_detail() == FrameType::CRYPTO ||
                            f.type.type_detail() == FrameType::HANDSHAKE_DONE) {
                            if (crypto.maybe_drop_handshake()) {
                                if (auto err = ackh.on_packet_number_space_discarded(ack::PacketNumberSpace::handshake)) {
                                    conn_err = std::move(err);
                                    return false;
                                }
                            }
                            if (crypto.handshake_confirmed()) {
                                ackh.set_handshake_confirmed();
                            }
                        }
                        return true;
                    })) {
                    return false;
                }
                if (status.is_ack_eliciting) {
                    unacked.add(ack::from_packet_type(summary.type), summary.packet_number);
                }
                return true;
            }

            bool parse_udp_payload(view::wvec data) {
                auto is_stateless_reset = [](packet::StatelessReset) { return false; };
                auto get_dstID_len = connIDs.get_dstID_len_callback();
                auto crypto_cb = [&](packetnum::Value pn, auto&& packet, view::wvec raw_packet) {
                    return handle_single_packet(packet::summarize(packet, pn), packet.payload, raw_packet.size());
                };
                auto decrypt_suite = crypto.decrypt_callback(conf.version, [&](PacketType type) {
                    return ackh.get_largest_pn(ack::from_packet_type(type));
                });
                auto plain_cb = [](PacketType, auto&& packet, view::wvec src) {
                    return true;
                };
                auto decrypt_err = [&](PacketType type, auto&& packet, packet::CryptoPacket cp, error::Error err, bool is_decrypted) {
                    logger.drop_packet(type, cp.packet_number, std::move(err));
                    return false;
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
    }  // namespace dnet::quic::context
}  // namespace utils

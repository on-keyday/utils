/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// context - quic context suite
#pragma once
#include "../ack/recv_history_interface.h"
#include "../ack/sent_history_interface.h"
#include "../status/status.h"
#include "../packet/creation.h"
#include "../packet/parse.h"
#include "../connid/id_manage.h"
#include "../path/dlpmtud.h"
#include "../path/verify.h"
#include "../path/path.h"
#include "../crypto/suite.h"
#include "../crypto/crypto_tag.h"
#include "../crypto/callback.h"
#include "../crypto/crypto.h"
#include "../crypto/decrypt.h"
#include "../transport_parameter/suite.h"
#include "../frame/parse.h"
#include "../stream/impl/conn.h"
#include "../log/log.h"
#include "../token/token.h"
#include "../close/close.h"
#include "../dgram/datagram.h"
#include "config.h"
#include <atomic>
#include "../type_macro.h"

namespace futils {
    namespace fnet::quic::context {

        enum class WriteFlag {
            normal = 0x0,
            mtu_probe = 0x1,
            conn_close = 0x2,
            path_probe = 0x3,
            path_migrate = 0x4,
            write_mode_mask = 0x0f,
            use_all_buffer = 0x10,
        };

        constexpr WriteFlag operator|(const WriteFlag& a, const WriteFlag& b) noexcept {
            return WriteFlag(std::uint32_t(a) | std::uint32_t(b));
        }

        constexpr bool operator&(const WriteFlag& a, const WriteFlag& b) noexcept {
            return std::uint32_t(a) & std::uint32_t(b);
        }

        constexpr auto err_creation = error::Error("error on creating packet. library bug!!", error::Category::lib, error::fnet_quic_implementation_bug);
        constexpr auto err_idle_timeout = error::Error("IDLE_TIMEOUT", error::Category::lib, error::fnet_quic_transport_error);
        constexpr auto err_handshake_timeout = error::Error("HANDSHAKE_TIMEOUT", error::Category::lib, error::fnet_quic_transport_error);

        struct CompareError {
            flex_storage expect, actual;
            void error(auto&& pb) {
                strutil::append(pb, "expect: ");
                for (auto c : expect) {
                    if (c < 0x10) {
                        strutil::append(pb, "0");
                    }
                    futils::number::to_string(pb, c, 16);
                }
                strutil::append(pb, " actual: ");
                for (auto c : actual) {
                    if (c < 0x10) {
                        strutil::append(pb, "0");
                    }
                    futils::number::to_string(pb, c, 16);
                }
            }
        };

        template <class T>
        concept resizable = requires(T t) {
            t.resize(1);
        };

        struct maybe_resizable_buffer {
            view::wvec buffer;

            void (*resize_)(void* ctx, std::size_t size, view::wvec& buffer) = nullptr;
            void* ctx = nullptr;

           private:
            template <class T>
            static void resize_stub(void* ctx, std::size_t size, view::wvec& buffer) {
                auto self = static_cast<T*>(ctx);
                self->resize(size);
                buffer = view::wvec{self->data(), self->size()};
            }

           public:
            constexpr maybe_resizable_buffer() = default;
            constexpr maybe_resizable_buffer(view::wvec buffer)
                : buffer(buffer) {}

            template <resizable T, helper_disable_self(maybe_resizable_buffer, T)>
            constexpr maybe_resizable_buffer(T&& buffer)
                : buffer(buffer), resize_(&resize_stub<std::decay_t<T>>), ctx(std::addressof(buffer)) {}

            void resize(std::size_t size) {
                if (resize_) {
                    resize_(ctx, size, buffer);
                }
            }
        };

        // Context is QUIC connection context suite
        // this class handles single connection both client and server
        // this class has no interest in about ip address and port
        // on client mode, this class has interest in Retry and 0-RTT tokens
        // on server mode, token validations are out of scope
        template <class TConfig>
        struct Context {
            using UserDefinedTypesConfig = typename TConfig::user_defined_types_config;

           private:
            QUIC_ctx_type(CongestionAlgorithm, TConfig, congestion_algorithm);
            QUIC_ctx_type(Lock, TConfig, context_lock);
            QUIC_ctx_type(DatagramDrop, TConfig, datagram_drop);
            QUIC_ctx_type(StreamTypeConfig, TConfig, stream_type_config);
            QUIC_ctx_type(ConnIDTypeConfig, TConfig, connid_type_config);
            QUIC_ctx_type(RecvPacketHistory, TConfig, recv_packet_history);
            QUIC_ctx_type(SentPacketHistory, TConfig, sent_packet_history);
            QUIC_ctx_vector_type(VersionsVec, TConfig, versions_vec);

            // QUIC runtime handlers
            Version version = version_negotiation;
            status::Status<CongestionAlgorithm> status;
            trsparam::TransportParameters params;
            crypto::CryptoSuite crypto;
            ack::SendHistoryInterface<SentPacketHistory> ackh;
            ack::RecvHistoryInterface<RecvPacketHistory> unacked;
            connid::IDManager<ConnIDTypeConfig> connIDs;
            token::RetryToken retry_token;
            token::ZeroRTTTokenManager zero_rtt_token;
            path::MTU mtu;
            path::PathVerifier path_verifier;
            close::Closer closer;
            std::atomic<close::CloseReason> closed;
            Lock close_locker;

            // application data handlers
            std::shared_ptr<stream::impl::Conn<StreamTypeConfig>> streams;
            std::shared_ptr<datagram::DatagramManager<Lock, DatagramDrop>> datagrams;

            // log
            log::ConnLogger logger;

            // internal parameters
            flex_storage packet_creation_buffer;

            // application protocol context
            std::shared_ptr<void> app_ctx;

            void reset_internal() {
                packet_creation_buffer.resize(0);
            }

            auto close_lock() {
                close_locker.lock();
                return helper::defer([&] {
                    close_locker.unlock();
                });
            }

            void set_quic_runtime_error(auto&& err) {
                const auto d = close_lock();
                closer.on_error(std::move(err), close::ErrorType::runtime);
                logger.report_error(closer.get_error());
            }

            void recv_close(const frame::ConnectionCloseFrame& conn_close) {
                const auto d = close_lock();
                if (closer.recv(conn_close)) {
                    logger.debug("connection close: CONNECTION_CLOSE by peer");
                    closed.store(close::CloseReason::close_by_remote);  // remote close
                    logger.report_error(closer.get_error());
                }
            }

            // apply_transport_param applies transport parameter from peer to local settings
            bool apply_transport_param(packet::PacketSummary summary) {
                if (status.transport_parameter_read()) {
                    return true;
                }
                auto data = crypto.tls().get_peer_quic_transport_params();
                if (!data) {
                    return true;  // nothing can do
                }
                binary::reader r{*data};
                auto accepted = crypto.tls().get_early_data_accepted();
                auto err = params.parse_peer(r, status.is_server(), accepted.has_value());
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                auto& peer = params.peer;
                // get registered initial_source_connection_id by peer
                auto connID = connIDs.choose_dstID(0);
                connIDs.on_transport_parameter_received(peer.active_connection_id_limit);
                // allow empty if empty
                if (peer.initial_src_connection_id != connID.id) {
                    set_quic_runtime_error(QUICError{
                        .msg = "initial_src_connection_id is not matched to id contained on initial packet",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .base = CompareError{
                            .expect = connID.id,
                            .actual = peer.initial_src_connection_id,
                        },
                    });
                    return false;
                }
                if (!status.is_server()) {  // client
                    if (peer.original_dst_connection_id != connIDs.get_original_dst_id()) {
                        set_quic_runtime_error(QUICError{
                            .msg = "original_dst_connection_id is not matched to id contained on initial packet",
                            .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                            .base = CompareError{
                                .expect = connIDs.get_original_dst_id(),
                                .actual = peer.original_dst_connection_id,
                            },
                        });
                        return false;
                    }
                    if (status.has_received_retry()) {
                        if (peer.retry_src_connection_id != connIDs.get_retry()) {
                            set_quic_runtime_error(QUICError{
                                .msg = "retry_src_connection_id is not matched to id previous received",
                                .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                                .base = CompareError{
                                    .expect = connIDs.get_retry(),
                                    .actual = peer.retry_src_connection_id,
                                },
                            });
                            return false;
                        }
                    }
                    connIDs.on_initial_stateless_reset_token_received(peer.stateless_reset_token);
                    if (!peer.preferred_address.connectionID.empty()) {
                        connIDs.on_preferred_address_received(peer.preferred_address.connectionID, peer.preferred_address.stateless_reset_token);
                    }
                }
                mtu.on_transport_parameter_received(peer.max_udp_payload_size);
                status.on_transport_parameter_received(peer.max_idle_timeout, peer.max_ack_delay, peer.ack_delay_exponent);
                auto by_peer = trsparam::to_initial_limits(peer);
                streams->apply_peer_initial_limits(by_peer);
                datagrams->on_transport_parameter(peer.max_datagram_frame_size);
                if (auto err = connIDs.issue_to_max()) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                status.on_transport_parameter_read();
                logger.debug("apply transport parameter");
                return true;
            }

            void reset_peer_transport_parameter() {
                params.peer = {};
            }

            // RFC9000 says:
            // A client MUST NOT use remembered values for the following parameters:
            // ack_delay_exponent, max_ack_delay, initial_source_connection_id,
            // original_destination_connection_id, preferred_address, retry_source_connection_id, and stateless_reset_token. The client MUST use the server's new values in the handshake instead;
            // if the server does not provide new values, the default values are used.
            void apply_0RTT_transport_parameter() {
                auto peer = trsparam::to_0RTT(params.peer);
                auto limits = trsparam::to_initial_limits(peer);
                streams->apply_peer_initial_limits(limits);
                connIDs.on_transport_parameter_received(peer.active_connection_id_limit);
                mtu.on_transport_parameter_received(peer.max_udp_payload_size);
                status.on_0RTT_transport_parameter(peer.max_idle_timeout);
                datagrams->on_transport_parameter(peer.max_datagram_frame_size);
                params.peer = trsparam::from_0RTT(peer);
                logger.debug("apply 0-RTT transport parameter");
            }

            // dropping handshake keys and crypto state
            void on_handshake_confirmed() {
                ackh.on_packet_number_space_discarded(status, status::PacketNumberSpace::handshake);
                logger.debug("drop handshake keys");
                status.on_handshake_confirmed();
                mtu.on_handshake_confirmed();
                unacked.on_packet_number_space_discarded(status::PacketNumberSpace::handshake);
                crypto.on_packet_number_space_discarded(status::PacketNumberSpace::handshake);
                logger.debug("handshake confirmed");
            }

            // dropping initial keys and crypto state
            void on_handshake_packet() {
                ackh.on_packet_number_space_discarded(status, status::PacketNumberSpace::initial);
                unacked.on_packet_number_space_discarded(status::PacketNumberSpace::initial);
                crypto.on_packet_number_space_discarded(status::PacketNumberSpace::initial);
                logger.debug("drop initial keys");
            }

            // write_frames calls each send() call of frame generator
            // this is used for normal packet writing
            bool write_frames(packet::PacketSummary& summary, ack::ACKRecorder& waiters, frame::fwriter& fw) {
                auto pn_space = status::from_packet_type(summary.type);
                auto wrap_io = [&](IOResult res) {
                    set_quic_runtime_error(QUICError{
                        .msg = to_string(res),
                        .frame_type = fw.prev_type,
                    });
                    return false;
                };
                if (status.is_pto_probe_required(pn_space) || status.should_send_any_packet()) {
                    logger.pto_fire(pn_space);
                    fw.write(frame::PingFrame{});
                }
                if (status.should_send_ping()) {
                    fw.write(frame::PingFrame{});
                }
                if (summary.type != PacketType::ZeroRTT) {
                    if (auto err = unacked.send(fw, pn_space, status.status_config(), summary.largest_ack);
                        err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                }
                if (status.is_on_congestion_limited()) {
                    return true;  // nothing to write
                }
                if (auto err = crypto.send(summary.type, summary.packet_number, waiters, fw);
                    err == IOResult::fatal) {
                    return wrap_io(err);
                }
                if (summary.type == PacketType::OneRTT) {
                    if (auto err = path_verifier.send_path_response(fw);
                        err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                }
                if (summary.type == PacketType::ZeroRTT ||
                    summary.type == PacketType::OneRTT) {
                    // on ZeroRTT situation, this doesn't send RetireConnectionID
                    if (auto err = connIDs.send(fw, waiters, summary.type == PacketType::OneRTT); err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                    if (auto err = datagrams->send(summary.packet_number, fw, waiters); err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                    if (auto err = streams->send(fw, waiters); err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                }
                return true;
            }

            // write_path_probe_frames writes PATH_CHALLANGE and set writing path
            bool write_path_probe_frames(packet::PacketSummary& summary, ack::ACKRecorder& waiters, frame::fwriter& fw, bool should_be_non_probe) {
                // TODO(on-keyday): can this write at 0-RTT?
                if (summary.type != PacketType::OneRTT) {
                    return true;  // nothing to write
                }
                auto wrap_io = [&](IOResult res) {
                    set_quic_runtime_error(QUICError{
                        .msg = to_string(res),
                        .frame_type = fw.prev_type,
                    });
                    return false;
                };
                auto [err, id] = path_verifier.send_path_challenge(fw, waiters, connIDs.random(), status.path_validation_deadline(), status.clock());
                if (err == IOResult::fatal) {
                    return wrap_io(err);
                }
                if (err != IOResult::ok) {
                    return true;  // nothing to write or no capacity
                }
                path_verifier.set_writing_path(id);
                if (should_be_non_probe) {
                    fw.write(frame::PingFrame{});  // must be non-probe
                    return write_frames(summary, waiters, fw);
                }
                else {
                    err = path_verifier.send_path_response(fw);
                    if (err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                    err = connIDs.send(fw, waiters, false);
                    if (err == IOResult::fatal) {
                        return wrap_io(err);
                    }
                }
                return true;
            }

            // render_payload_callback returns render_payload callback for packet::create_packet
            // this calls write_frame and add paddings for encryption
            auto render_payload_callback(packet::PacketSummary& summary, ack::ACKRecorder& wait, bool& no_payload, packet::PacketStatus& pstatus, bool fill_all) {
                return [&, fill_all](binary::writer& w, packetnum::WireVal wire) {
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
                    logger.sending_packet(path_verifier.get_writing_path(), summary, w.written());  // logging
                    return true;
                };
            }

            // save_sent_packet saves information about packet that has just sent
            bool save_sent_packet(packet::PacketSummary summary, packet::PacketStatus pstatus, std::uint64_t sent_bytes, ack::ACKRecorder& waiters) {
                const auto pn_space = status::from_packet_type(summary.type);
                ack::SentPacket sent;
                sent.packet_number = summary.packet_number;
                sent.type = summary.type;
                sent.status = pstatus;
                sent.sent_bytes = sent_bytes;
                // sent.waiters = std::move(waiters);
                sent.waiter = waiters.record();
                sent.largest_ack = summary.largest_ack;
                auto err = ackh.on_packet_sent(status, pn_space, std::move(sent));
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    ack::mark_as_lost(sent.waiter);
                    return false;
                }
                crypto.on_packet_sent(summary.type, summary.packet_number);
                return true;
            }

            // waiters is needed for MTU Probe packet
            bool write_packet(binary::writer& w, packet::PacketSummary summary, WriteFlag flags, ack::ACKRecorder& waiters) {
                auto wmode = WriteFlag(std::uint32_t(flags) & std::uint32_t(WriteFlag::write_mode_mask));
                auto set_error = [&](auto&& err) {
                    if (wmode == WriteFlag::conn_close) {
                        return;
                    }
                    set_quic_runtime_error(std::move(err));
                };
                // get packet number space
                auto pn_space = status::from_packet_type(summary.type);
                if (pn_space == status::PacketNumberSpace::no_space) {
                    set_error(error::Error("invalid packet type", error::Category::lib, error::fnet_quic_implementation_bug));
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
                    summary.key_bit = suite.phase == crypto::KeyPhase::one;
                }

                // define frame renderer arguments
                bool no_payload = false;
                packet::PacketStatus pstatus;
                auto mark_lost = helper::defer([&] {
                    std::weak_ptr<ack::ACKLostRecord> r = waiters.get();
                    ack::mark_as_lost(r);
                });

                // use copy
                // restore when payload rendered and succeeded
                auto copy = w.clone();

                // render packet according to write mode
                packet::CryptoPacket plain;
                bool rendered = false;

                const auto auth_tag_len = crypto::authentication_tag_length;

                switch (wmode) {
                    case WriteFlag::conn_close: {
                        std::tie(plain, rendered) = packet::create_packet(
                            copy, summary, largest_acked,
                            auth_tag_len,
                            flags & WriteFlag::use_all_buffer, [&](binary::writer& w, packetnum::WireVal wire) {
                                frame::fwriter fw{w};
                                if (!closer.send(fw, summary.type)) {
                                    return false;
                                }
                                frame::add_padding_for_encryption(fw, wire, crypto::sample_skip_size);
                                pstatus = fw.status;
                                logger.sending_packet(path_verifier.get_writing_path(), summary, w.written());
                                return true;
                            });
                        break;
                    }
                    case WriteFlag::mtu_probe: {
                        std::tie(plain, rendered) = packet::create_packet(
                            copy, summary, largest_acked,
                            auth_tag_len, true, [&](binary::writer& w, packetnum::WireVal wire) {
                                frame::fwriter fw{w};
                                fw.write(frame::PingFrame{});
                                pstatus = fw.status;
                                pstatus.set_mtu_probe();  // is MTU probe
                                logger.sending_packet(path_verifier.get_writing_path(), summary, w.written());
                                return true;
                            });
                        break;
                    }
                    case WriteFlag::path_probe:
                    case WriteFlag::path_migrate: {
                        std::tie(plain, rendered) = packet::create_packet(
                            copy, summary, largest_acked,
                            auth_tag_len, flags & WriteFlag::use_all_buffer,
                            [&](binary::writer& w, packetnum::WireVal wire) {
                                frame::fwriter fw{w};
                                if (!write_path_probe_frames(summary, waiters, fw, wmode == WriteFlag::path_probe)) {
                                    return false;
                                }
                                if (w.written().size() == 0) {
                                    no_payload = true;
                                    return false;
                                }
                                pstatus = fw.status;
                                logger.sending_packet(path_verifier.get_writing_path(), summary, w.written());
                                return true;
                            });
                    }
                    case WriteFlag::normal: {
                        const auto fill_all = flags & WriteFlag::use_all_buffer;
                        std::tie(plain, rendered) = packet::create_packet(
                            copy, summary, largest_acked,
                            auth_tag_len, fill_all,
                            render_payload_callback(summary, waiters, no_payload, pstatus, fill_all));
                        break;
                    }
                    default: {
                        set_error(error::Error("unexpected write mode. library bug!!", error::Category::lib, error::fnet_quic_implementation_bug));
                        return false;
                    }
                }

                // on connections close,
                // closer always has error
                if (wmode != WriteFlag::conn_close) {
                    if (closer.has_error()) {
                        return false;
                    }
                }
                if (no_payload) {
                    return true;  // skip. no payload exists
                }
                if (!rendered) {
                    // set_error(err_creation);
                    // return false;
                    return true;  // no space to write
                }

                // encrypt packet
                auto err = crypto::encrypt_packet(*suite.keyiv, *suite.hp, *suite.cipher, plain);
                if (err) {
                    set_error(std::move(err));
                    return false;
                }

                mark_lost.cancel();

                if (wmode != WriteFlag::conn_close) {
                    // save packet
                    if (!save_sent_packet(summary, pstatus, plain.src.size(), waiters)) {
                        return false;
                    }
                }

                status.consume_packet_number(pn_space);
                connIDs.on_packet_sent();
                w = std::move(copy);  // restore

                return true;
            }

            bool pickup_dstID(view::rvec& dstID) {
                auto [id, err] = connIDs.pick_up_dstID(status.is_server(), status.handshake_status().handshake_complete(), params.local.active_connection_id_limit);
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                dstID = id;
                return true;
            }

            // write_initial generates initial packet
            bool write_initial(binary::writer& w, WriteFlag mode) {
                packet::PacketSummary summary;
                summary.type = PacketType::Initial;
                summary.srcID = connIDs.pick_up_srcID();
                if (!pickup_dstID(summary.dstID)) {
                    return false;
                }
                if (status.has_received_retry()) {
                    summary.token = retry_token.get_token();
                }
                else {
                    auto ztoken = zero_rtt_token.find();
                    if (!ztoken.token.null()) {
                        summary.token = ztoken.token;
                    }
                }
                ack::ACKRecorder waiter;
                if (!write_packet(w, summary, mode, waiter)) {
                    return false;
                }
                return true;
            }

            // write_handshake generates handshake packet
            // if succeeded, may drop initial key
            bool write_handshake(binary::writer& w, WriteFlag mode) {
                packet::PacketSummary summary;
                summary.type = PacketType::Handshake;
                summary.srcID = connIDs.pick_up_srcID();
                if (!pickup_dstID(summary.dstID)) {
                    return false;
                }
                ack::ACKRecorder waiter;
                if (!write_packet(w, summary, mode, waiter)) {
                    return false;
                }
                if (!status.is_server() && crypto.maybe_drop_initial()) {
                    on_handshake_packet();
                }
                return true;
            }

            // write_onertt generates 1-RTT packet
            // waiter is used for MTU probe
            bool write_onertt(binary::writer& w, WriteFlag mode, ack::ACKRecorder& waiter) {
                packet::PacketSummary summary;
                summary.type = PacketType::OneRTT;
                if (!pickup_dstID(summary.dstID)) {
                    return false;
                }
                if (!write_packet(w, summary, mode, waiter)) {
                    return false;
                }
                return true;
            }

            // write_zerortt generates 0-RTT packet
            bool write_zerortt(binary::writer& w, WriteFlag mode) {
                if (status.is_server()) {
                    return true;
                }
                packet::PacketSummary summary;
                summary.type = PacketType::ZeroRTT;
                summary.srcID = connIDs.pick_up_srcID();
                if (!pickup_dstID(summary.dstID)) {
                    return false;
                }
                ack::ACKRecorder waiter;
                if (!write_packet(w, summary, mode, waiter)) {
                    return false;
                }
                return true;
            }

            binary::writer prepare_writer(maybe_resizable_buffer external_buffer, std::uint64_t bufsize) {
                external_buffer.resize(bufsize);
                if (external_buffer.buffer.size() >= bufsize) {
                    return binary::writer{external_buffer.buffer.substr(0, bufsize)};
                }
                packet_creation_buffer.resize(bufsize);
                return binary::writer{packet_creation_buffer};
            }

            std::pair<view::rvec, bool> create_encrypted_packet(bool on_close, maybe_resizable_buffer external_buffer) {
                if (!status.is_server()) {
                    // on client drop 0-RTT key if 1-RTT key is installed
                    crypto.maybe_drop_0rtt();
                }
                auto w = prepare_writer(external_buffer, mtu.current_datagram_size());
                const bool has_initial = crypto.write_installed(PacketType::Initial) &&
                                         status.can_send(status::PacketNumberSpace::initial);
                bool has_handshake = crypto.write_installed(PacketType::Handshake) &&
                                     status.can_send(status::PacketNumberSpace::handshake);
                const bool has_onertt = crypto.write_installed(PacketType::OneRTT) &&
                                        status.can_send(status::PacketNumberSpace::application);
                const bool has_zerortt = !status.is_server() &&  // client
                                         crypto.write_installed(PacketType::ZeroRTT) &&
                                         status.can_send(status::PacketNumberSpace::application) &&
                                         !has_onertt &&
                                         !on_close;  // cannot send CONNECTION_CLOSE
                const auto to_mode = [&](bool b) {
                    if (on_close) {
                        return b ? WriteFlag::use_all_buffer | WriteFlag::conn_close : WriteFlag::conn_close;
                    }
                    else {
                        return b ? WriteFlag::use_all_buffer : WriteFlag::normal;
                    }
                };

                if (has_initial) {
                    if (!write_initial(w, to_mode(!has_handshake && !has_zerortt && !has_onertt))) {
                        return {{}, false};
                    }
                }
                if (has_zerortt) {
                    if (!write_zerortt(w, to_mode(!has_handshake))) {
                        return {{}, false};
                    }
                }
                if (!on_close && has_handshake && crypto.maybe_drop_handshake()) {
                    on_handshake_confirmed();
                    has_handshake = false;
                }
                if (has_handshake) {
                    if (!write_handshake(w, to_mode(has_initial && !has_onertt))) {
                        return {{}, false};
                    }
                }
                if (has_onertt) {
                    ack::ACKRecorder waiter;
                    if (!write_onertt(w, to_mode(has_initial), waiter)) {
                        return {{}, false};
                    }
                }
                if (!on_close) {
                    logger.loss_timer_state(status.loss_timer(), status.now());
                }
                return {w.written(), true};
            }

            // creates PATH_CHALLANGE
            // maybe change writing_path
            std::pair<view::rvec, bool> create_path_probe_packet(maybe_resizable_buffer external_buffer) {
                if (!status.handshake_confirmed() ||
                    !crypto.write_installed(PacketType::OneRTT) ||
                    !status.can_send(status::PacketNumberSpace::application)) {
                    return {{}, true};  // can't do
                }
                if (!path_verifier.has_probe_to_send()) {
                    // detecting probe timeout
                    path_verifier.detect_path_probe_timeout(status.clock());
                    return {{}, true};  // needless to do
                }
                // TODO(on-keyday):
                // we are willing to do DPLPMTUD with path probe
                // but currently use fixed 1200 byte
                auto w = prepare_writer(external_buffer, path::initial_udp_datagram_size);
                ack::ACKRecorder waiter;
                if (!write_onertt(w, WriteFlag::path_probe, waiter)) {
                    return {{}, false};
                }
                logger.loss_timer_state(status.loss_timer(), status.now());
                return {w.written(), true};
            }

            std::pair<view::rvec, bool> create_mtu_probe_packet(maybe_resizable_buffer external_buffer) {
                const bool has_onertt = crypto.write_installed(PacketType::OneRTT) &&
                                        status.can_send(status::PacketNumberSpace::application);
                if (!has_onertt) {
                    return {{}, true};  // nothing to do
                }
                ack::ACKRecorder waiter;
                auto [size, required] = mtu.probe_required(waiter);
                if (!required) {
                    return {{}, true};  // nothing to do too
                }
                auto w = prepare_writer(external_buffer, size);
                if (!write_onertt(w, WriteFlag::mtu_probe, waiter)) {
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

            std::pair<view::rvec, bool> create_connection_close_packet(maybe_resizable_buffer external_buffer) {
                const auto d = close_lock();
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
                    closer.on_close_retransmitted();
                    return {sent, true};
                }
                if (closer.close_by_peer()) {
                    return {{}, false};  // close by peer
                }
                auto [packet, ok] = create_encrypted_packet(true, external_buffer);
                if (!ok) {
                    logger.debug("fatal error!! connection close packet creation failure");
                    return {{}, false};  // fatal error!!!!!
                }
                closer.on_close_packet_sent(packet);
                status.set_close_timer();
                logger.debug("connection close: CONNECTION_CLOSE by local");
                closed.store(
                    closer.error_type() == close::ErrorType::app
                        ? close::CloseReason::close_by_local_app
                        : close::CloseReason::close_by_local_runtime);  // local close
                return {packet, true};
            }

            // set tranport parameter sets transport parameter to TLS layer
            // client: before connection start
            // server: on first initial packet received
            bool set_transport_parameter() {
                // set transport parameter
                packet_creation_buffer.resize(1200);
                binary::writer w{packet_creation_buffer};
                if (!params.render_local(w, status.is_server(), status.has_sent_retry())) {
                    set_quic_runtime_error(error::Error("failed to render transport params", error::Category::lib, error::fnet_quic_implementation_bug));
                    return false;
                }
                auto p = w.written();
                auto err = crypto.tls().set_quic_transport_params(p).error_or(error::none);
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                return true;
            }

            bool conn_timeout() {
                if (status.is_idle_timeout()) {
                    if (status.handshake_confirmed()) {
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
                if (frame.type.type() == FrameType::PADDING ||
                    frame.type.type() == FrameType::PING) {
                    return true;
                }
                if (auto ack = frame::cast<frame::ACKFrame<slib::vector>>(&frame)) {
                    ack::RemovedPackets removed_packets;
                    auto err = ackh.on_ack_received(status, status::from_packet_type(summary.type), removed_packets, nullptr, *ack, [&] {
                        stream::impl::Conn<StreamTypeConfig>* ptr = streams.get();
                        return ptr->is_flow_control_limited();
                    });
                    if (err) {
                        set_quic_runtime_error(std::move(err));
                        return false;
                    }
                    if (summary.type == PacketType::OneRTT) {
                        unacked.delete_onertt_under(status.get_onertt_largest_acked_sent_ack());
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
                    stream::impl::Conn<StreamTypeConfig>* ptr = streams.get();
                    error::Error tmp_err;
                    std::tie(std::ignore, tmp_err) = ptr->recv(frame);
                    if (tmp_err) {
                        set_quic_runtime_error(std::move(tmp_err));
                        return false;
                    }
                    return true;
                }
                if (frame.type.type() == FrameType::DATAGRAM) {
                    auto err = datagrams->recv(summary.packet_number, static_cast<const frame::DatagramFrame&>(frame));
                    if (err) {
                        set_quic_runtime_error(std::move(err));
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
                    auto err = path_verifier.recv(frame);
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
                if (frame.type.type() == FrameType::NEW_TOKEN) {
                    if (status.is_server()) {
                        set_quic_runtime_error(QUICError{
                            .msg = "NewTokenFrame from client",
                            .transport_error = TransportError::PROTOCOL_VIOLATION,
                            .frame_type = FrameType::NEW_TOKEN,
                        });
                        return false;
                    }
                    auto token = static_cast<const frame::NewTokenFrame&>(frame).token;
                    zero_rtt_token.store(token::Token{.token = token});
                    return true;
                }
                set_quic_runtime_error(error::Error("unexpected frame type!", error::Category::lib, error::fnet_quic_implementation_bug));
                return false;
            }

            bool handle_on_initial(packet::PacketSummary summary) {
                if (summary.type == PacketType::Initial) {
                    if (!connIDs.initial_conn_id_accepted()) {
                        if (auto err = connIDs.on_initial_packet_received(summary.srcID)) {
                            set_quic_runtime_error(std::move(err));
                            return false;
                        }
                        if (status.is_server()) {
                            byte dum = 0;
                            view::rvec srcID{&dum, 0};
                            if (!connIDs.local_using_zero_length()) {
                                if (!issue_seq0_connid(&srcID)) {
                                    return false;
                                }
                            }
                            logger.debug("save client connection ID");
                            if (status.has_sent_retry()) {
                                params.local.retry_src_connection_id = srcID;
                            }
                            else {
                                params.local.initial_src_connection_id = srcID;
                            }
                            if (!set_transport_parameter()) {
                                return false;  // failure
                            }
                        }
                        else {
                            logger.debug("save server connection ID");
                        }
                    }
                }
                return true;
            }

            bool check_connID(packet::PacketSummary& prev, packet::PacketSummary sum, view::rvec raw_packet) {
                constexpr auto invalid_src_id = error::Error("invalid srcID", error::Category::lib, error::fnet_quic_connection_id_error);
                constexpr auto invalid_dst_id = error::Error("invalid dstID", error::Category::lib, error::fnet_quic_connection_id_error);
                if (prev.type != PacketType::Unknown) {
                    if (sum.dstID != prev.dstID) {
                        logger.drop_packet(sum.type, sum.packet_number, error::Error("using not same dstID", error::Category::lib, error::fnet_quic_connection_id_error), raw_packet, false);
                        return false;
                    }
                    if (sum.type != PacketType::OneRTT) {
                        if (sum.srcID != prev.srcID) {
                            logger.drop_packet(sum.type, sum.packet_number, error::Error("using not same srcID", error::Category::lib, error::fnet_quic_connection_id_error), raw_packet, false);
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
                                logger.drop_packet(sum.type, sum.packet_number, invalid_src_id, raw_packet, false);
                                return false;
                            }
                        }
                        if (!connIDs.has_srcID(sum.dstID)) {
                            logger.drop_packet(sum.type, sum.packet_number, invalid_dst_id, raw_packet, false);
                            return false;
                        }
                    }
                    else {  // server
                        if (sum.type != PacketType::Initial ||
                            connIDs.initial_conn_id_accepted()) {
                            if (sum.type != PacketType::OneRTT) {
                                if (!connIDs.has_dstID(sum.srcID)) {
                                    logger.drop_packet(sum.type, sum.packet_number, invalid_dst_id, raw_packet, false);
                                    return false;
                                }
                            }
                            if (!connIDs.has_srcID(sum.dstID)) {
                                logger.drop_packet(sum.type, sum.packet_number, invalid_src_id, raw_packet, false);
                                return false;
                            }
                        }
                    }
                }
                prev = sum;
                return true;
            }

            bool handle_single_packet(packet::PacketSummary summary, view::rvec payload, path::PathID path_id) {
                auto pn_space = status::from_packet_type(summary.type);
                if (summary.type == PacketType::OneRTT) {
                    summary.version = version;
                }
                status.on_packet_decrypted(pn_space);
                logger.recv_packet(path_id, summary, payload);
                if (!handle_on_initial(summary)) {
                    return false;
                }
                if (status.is_server()) {
                    connIDs.maybe_retire_original_dst_id(summary.dstID);
                }
                binary::reader r{payload};
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
                        pstatus.add_frame(f.type.type_detail());
                        if (!handle_frame(summary, f)) {
                            return false;
                        }
                        return true;
                    })) {
                    return false;
                }
                if (summary.type == PacketType::Handshake && crypto.maybe_drop_initial()) {
                    on_handshake_packet();
                }
                if (summary.type == PacketType::OneRTT && crypto.maybe_drop_handshake()) {
                    on_handshake_confirmed();
                }
                unacked.on_packet_processed(pn_space, summary.packet_number, pstatus.is_ack_eliciting(), status.status_config());
                status.on_packet_processed(pn_space, summary.packet_number);
                path_verifier.maybe_peer_migrated_path(path_id, pstatus.is_non_path_probe());
                logger.loss_timer_state(status.loss_timer(), status.now());
                return true;
            }

            bool handle_retry_packet(packet::RetryPacket retry, view::rvec raw_packet, path::PathID path_id) {
                logger.debug("received Retry packet");
                logger.recv_packet(path_id, packet::summarize(retry), {});
                if (status.is_server()) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("received Retry packet at server", error::Category::lib, error::fnet_quic_packet_error), raw_packet, true);
                    return true;
                }
                if (status.handshake_status().has_received_packet()) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("unexpected Retry packet after receiving packet", error::Category::lib, error::fnet_quic_packet_error), raw_packet, true);
                    return true;
                }
                if (status.has_received_retry()) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("unexpected Retry packet after receiving Retry", error::Category::lib, error::fnet_quic_packet_error), raw_packet, true);
                    return true;
                }
                packet::RetryPseudoPacket pseudo;
                pseudo.from_retry_packet(connIDs.get_original_dst_id(), retry);
                packet_creation_buffer.resize(pseudo.len());
                binary::writer w{packet_creation_buffer};
                if (!pseudo.render(w)) {
                    set_quic_runtime_error(error::Error("failed to render Retry packet. library bug!!", error::Category::lib, error::fnet_quic_implementation_bug));
                    return false;
                }
                byte output[16];
                auto err = crypto::generate_retry_integrity_tag(output, w.written(), version);
                if (err) {
                    set_quic_runtime_error(err);
                    return false;
                }
                if (view::rvec(retry.retry_integrity_tag) != output) {
                    logger.drop_packet(PacketType::Retry, packetnum::infinity, error::Error("retry integrity tags are not matched. maybe observer exists or packet broken", error::Category::lib, error::fnet_quic_packet_error), raw_packet, true);
                    return true;
                }
                ackh.on_retry_received(status);
                connIDs.on_retry_received(retry.srcID);
                retry_token.on_retry_received(retry.retry_token);
                crypto.install_initial_secret(version, retry.srcID);

                logger.debug("retry packet accepted");
                return true;
            }

            bool issue_seq0_connid(view::rvec* first = nullptr) {
                auto [issued, err] = connIDs.issue(false);
                if (err) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                if (issued.seq != 0) {
                    set_quic_runtime_error(error::Error("failed to issue connection ID with sequence number 0. library bug!!", error::Category::lib, error::fnet_quic_implementation_bug));
                    return false;
                }
                if (first) {
                    *first = issued.id;
                }
                return true;
            }

           public:
            // thread safe call
            bool is_closed() const {
                return closed != close::CloseReason::not_closed;
            }

            // thread safe call
            close::CloseReason close_reason() const {
                return closed;
            }

            // thread unsafe call
            bool handshake_complete() const {
                return status.handshake_complete();
            }

            // thread unsafe call
            bool handshake_confirmed() const {
                return status.handshake_confirmed();
            }

            // thread safe call
            // if err is QUICError, use detail of it
            // otherwise wrap error
            void request_close(error::Error err) {
                if (closer.has_error()) {
                    return;  // already set
                }

                if (auto qerr = err.as<QUICError>()) {
                    qerr->is_app = true;
                    qerr->by_peer = false;
                }
                else if (auto aerr = err.as<AppError>()) {
                    // nothing to do
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
                logger.report_error(closer.get_error());
            }

            // thread unsafe call
            const error::Error& get_conn_error() const {
                return closer.get_error();
            }

            // thread unsafe call
            auto get_streams() const {
                return streams;
            }

            // thread unsafe call
            auto get_datagrams() const {
                return datagrams;
            }

            // thread unsafe call
            auto get_multiplexer_ptr() const {
                return connIDs.get_exporter_mux();
            }

            // thread unsafe call
            void set_outer_self_ptr(std::weak_ptr<void>&& m) {
                connIDs.set_exporter_obj(std::move(m));
            }

            // thread unsafe call
            auto get_outer_self_ptr() const {
                return connIDs.get_exporter_obj();
            }

            auto get_application_context() const {
                return app_ctx;
            }

            void set_application_context(std::shared_ptr<void>&& ctx) {
                app_ctx = std::forward<decltype(ctx)>(ctx);
            }

            void set_trace_id(view::rvec id) {
                logger.trace_id = id;
            }

            view::rvec get_trace_id() const {
                return logger.trace_id;
            }

            // get earliest timer deadline
            // thread unsafe call
            time::Time get_earliest_deadline() const {
                return status.get_earliest_deadline(unacked.get_deadline());
            }

            // thread unsafe call
            bool init(const Config& config, UserDefinedTypesConfig&& udconfig = {}) {
                // reset internal parameters
                reset_internal();
                closer.reset();

                // reset ack handler
                ackh.reset();
                unacked.reset();

                version = config.version;

                // setup TLS
                error::Error tmp_err;
                auto tls = tls::create_quic_tls_with_error(config.tls_config, crypto::qtls_callback, &crypto);
                if (!tls) {
                    set_quic_runtime_error(std::move(tls.error()));
                    return false;
                }
                if (config.session) {
                    tls->set_session(std::move(config.session));
                }
                crypto.reset(std::move(*tls), config.server);

                // initialize connection status
                status::InternalConfig internal;
                static_cast<status::Config&>(internal) = config.internal_parameters;
                internal.local_ack_delay_exponent = config.transport_parameters.ack_delay_exponent;
                internal.idle_timeout = config.transport_parameters.max_idle_timeout;
                internal.local_max_ack_delay = config.transport_parameters.max_ack_delay;
                mtu.reset(config.path_parameters.mtu);
                status.reset(internal, std::move(udconfig.algorithm), config.server, mtu.current_datagram_size());
                if (!status.now().valid()) {
                    set_quic_runtime_error(QUICError{
                        .msg = "clock.now() returns invalid time. clock must be set",
                    });
                    return false;
                }

                // set transport parameters
                params.local = std::move(config.transport_parameters);

                // now if we are client, remaining peer transport parameter for 0-RTT
                // will reset when connect_start
                params.peer_checker = {};
                if (config.server) {
                    params.peer = {};  // reset when server; no 0-RTT exists
                }

                // setup connection IDs manager
                connIDs.reset(config.version, std::move(config.connid_parameters));

                auto local = trsparam::to_initial_limits(params.local);

                // create streams
                streams = stream::impl::make_conn<StreamTypeConfig>(config.server
                                                                        ? stream::Origin::server
                                                                        : stream::Origin::client,
                                                                    get_outer_self_ptr().lock()
                                                                    /*for server*/);

                streams->apply_local_initial_limits(local);
                logger = std::move(config.logger);

                // setup token storage
                retry_token.reset();
                zero_rtt_token.reset(config.zero_rtt);

                // setup datagram handler
                using DgramT = datagram::DatagramManager<Lock, DatagramDrop>;
                datagrams = std::allocate_shared<DgramT>(glheap_allocator<DgramT>{});
                datagrams->reset(config.transport_parameters.max_datagram_frame_size, config.datagram_parameters,
                                 get_outer_self_ptr().lock(),  // for server
                                 std::move(udconfig.dgram_drop));

                // setup path validation status
                path_verifier.reset(config.path_parameters.original_path, config.path_parameters.max_path_challenge);

                // finalize initialization
                closed.store(close::CloseReason::not_closed);
                return true;
            }

            // thread unsafe call
            bool connect_start(const char* hostname = nullptr) {
                if (status.handshake_started()) {
                    return false;
                }
                if (status.is_server()) {
                    return false;
                }
                status.on_handshake_start();
                if (!connIDs.local_using_zero_length()) {
                    if (!issue_seq0_connid()) {
                        return false;
                    }
                    auto id = connIDs.choose_srcID(0);
                    params.set_local(trsparam::make_transport_param(trsparam::DefinedID::initial_src_connection_id, id.id));
                }
                else {
                    params.set_local(trsparam::make_transport_param(trsparam::DefinedID::initial_src_connection_id, {}));
                }

                if (!connIDs.gen_original_dst_id()) {
                    set_quic_runtime_error(error::Error("failed to generate client destination connection ID. library bug!!", error::Category::lib, error::fnet_quic_implementation_bug));
                    return false;
                }
                if (!crypto.install_initial_secret(version, connIDs.get_original_dst_id())) {
                    return false;
                }
                if (!set_transport_parameter()) {
                    return false;
                }
                if (hostname) {
                    // enable SNI
                    if (auto err = crypto.tls().set_hostname(hostname).error_or(error::none)) {
                        set_quic_runtime_error(std::move(err));
                        return false;
                    }
                }
                auto err = crypto.tls().connect().error_or(error::none);
                if (err && !tls::isTLSBlock(err)) {
                    set_quic_runtime_error(std::move(err));
                    return false;
                }
                if (!crypto.write_installed(PacketType::ZeroRTT)) {
                    reset_peer_transport_parameter();
                }
                else {
                    apply_0RTT_transport_parameter();
                }
                logger.debug("client handshake start");
                return true;
            }

            // thread unsafe call
            // if retry_src_conn_id is not null, it is set to initial_src_connection_id
            // and next initial packet's src connection id will be retry_src_connection_id
            // otherwise next initial packet's src connection id will be initial_src_connection_id
            bool accept_start(view::rvec original_dst_id, view::rvec retry_src_conn_id = {}) {
                if (status.handshake_started()) {
                    return false;
                }
                if (!status.is_server()) {
                    return false;
                }
                status.on_handshake_start();
                params.local.original_dst_connection_id = original_dst_id;
                if (!retry_src_conn_id.null()) {
                    params.local.retry_src_connection_id = retry_src_conn_id;
                    status.set_retry_sent();
                }
                connIDs.set_original_dst_id(original_dst_id, retry_src_conn_id);
                // needless to call tls.accept here
                // because no handshake data is available here
                logger.debug("server handshake start");
                return true;
            }

            // create udp payload of QUIC packets
            // if external_buffer is not null, use it as buffer
            // otherwise use internal buffer
            // if payload is empty, no packet is created
            // if idle is false, connection is terminated
            // thread unsafe call
            // returns (payload,path_id,idle)
            std::tuple<view::rvec, path::PathID, bool> create_udp_payload(maybe_resizable_buffer external_buffer = {}) {
                auto create_close = [&] {
                    auto [payload, idle] = create_connection_close_packet(external_buffer);
                    return std::tuple{payload, path_verifier.get_writing_path(), idle};
                };
                if (closer.has_error()) {
                    return create_close();
                }
                if (conn_timeout()) {
                    return {view::rvec(), path::PathID{}, false};
                }
                if (!maybe_loss_timeout()) {
                    // error on loss detection
                    return create_close();
                }
                path_verifier.set_writing_path_to_active_path();
                view::rvec packet;
                bool ok;
                std::tie(packet, ok) = create_path_probe_packet(external_buffer);
                if (!ok) {
                    return create_close();
                }
                if (packet.size()) {
                    return {packet, path_verifier.get_writing_path(), true};
                }
                // reset path id for active path
                path_verifier.set_writing_path_to_active_path();
                std::tie(packet, ok) = create_encrypted_packet(false, external_buffer);
                if (!ok) {
                    return create_close();
                }
                if (packet.size()) {
                    return {packet, path_verifier.get_writing_path(), ok};
                }
                std::tie(packet, ok) = create_mtu_probe_packet(external_buffer);
                if (!ok) {
                    return create_close();
                }
                return {packet, path_verifier.get_writing_path(), true};
            }

            constexpr path::PathID get_active_path() const {
                return path_verifier.get_writing_path();
            }

            // thread unsafe call
            // returns (should_send)
            bool parse_udp_payload(view::wvec data, path::PathID path = path::original_path) {
                if (!closer.sent_packet().null()) {
                    closer.on_recv_after_close_sent();
                    return true;  // ignore
                }
                auto is_stateless_reset = connIDs.get_stateless_reset_callback();
                auto get_dstID_len = connIDs.get_dstID_len_callback();
                packet::PacketSummary prev;
                prev.type = PacketType::Unknown;
                auto crypto_cb = [&](packetnum::Value pn, auto&& packet, view::wvec raw_packet) {
                    packet::PacketSummary sum = packet::summarize(packet, pn);
                    if (unacked.is_duplicated(status::from_packet_type(sum.type), pn)) {
                        logger.drop_packet(sum.type, pn, error::Error("duplicated packet", error::Category::lib, error::fnet_quic_packet_error), raw_packet, true);
                        return true;  // ignore
                    }
                    return handle_single_packet(sum, packet.payload, path);
                };
                auto check_connID_cb = [&](auto&& packet, view::rvec raw_packet) {
                    packet::PacketSummary sum = packet::summarize(packet, packetnum::infinity);
                    bool first_time = prev.type == PacketType::Unknown;
                    if (!check_connID(prev, sum, raw_packet)) {
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
                auto plain_cb = [this, &path](PacketType type, auto&& packet, view::wvec src) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(packet)>, packet::RetryPacket>) {
                        return handle_retry_packet(packet, src, path);
                    }
                    return true;
                };
                auto decrypt_err = [&](PacketType type, auto&& packet, packet::CryptoPacket cp, error::Error err, bool is_decrypted) {
                    logger.drop_packet(type, cp.packet_number, std::move(err), cp.src, is_decrypted);
                    // decrypt failure doesn't make packet parse failure
                    // see https://tex2e.github.io/rfc-translater/html/rfc9000#12-2--Coalescing-Packets
                    return true;
                };
                auto plain_err = [&](PacketType type, auto&& packet, view::wvec src, bool err, bool valid_type) {
                    logger.drop_packet(type, packetnum::infinity, error::Error("plain packet failure", error::Category::lib, error::fnet_quic_packet_error), src, !is_ProtectedPacket(type));
                    return true;  // ignore error
                };
                auto ok = crypto::parse_with_decrypt<VersionsVec>(
                    data, crypto::authentication_tag_length, is_stateless_reset, get_dstID_len,
                    crypto_cb, decrypt_suite, plain_cb, decrypt_err, plain_err, check_connID_cb);
                if (!ok) {
                    return true;
                }
                return true;
            }

            // expose_closed_context expose context required to close
            // after call this, Context become invalid
            // this returns true when ClosedContext is enabled otherwise false
            // ids are always set
            bool expose_closed_context(close::ClosedContext& ctx, auto&& ids) {
                const auto l = close_lock();
                connIDs.expose_close_data(ids);
                switch (closed.load()) {
                    case close::CloseReason::not_closed:
                        logger.debug("warning: expose_closed_context is called but connection is not closed");
                        return false;
                    case close::CloseReason::idle_timeout:
                    case close::CloseReason::handshake_timeout:
                        ctx.clock = status.clock();
                        ctx.close_timeout.set_deadline(ctx.clock.now());
                        ctx.active_path = path_verifier.get_writing_path();
                        logger.debug("expose close context: idle or handshake timeout");
                        return false;
                    default:
                        break;
                }
                ctx = closer.expose_closed_context(status.clock(), status.get_close_deadline());
                ctx.active_path = path_verifier.get_writing_path();
                logger.debug("expose close context: normal close");
                return true;
            }

            // thread unsafe call
            constexpr bool transport_parameter_read() const {
                return status.transport_parameter_read();
            }

            // thread unsafe call
            // reports whether migration is enabled for this connection
            constexpr bool migration_enabled() const {
                return status.transport_parameter_read() &&
                       // RFC9000 9. Connection Migration says:
                       // An endpoint MUST NOT initiate connection migration before
                       // the handshake is confirmed, as defined in Section 4.1.2 of [QUIC-TLS].
                       // see also https://datatracker.ietf.org/doc/html/rfc9000#name-connection-migration
                       status.handshake_confirmed() &&
                       !params.peer.disable_active_migration;
            }

            // thread unsafe call
            view::rvec get_selected_alpn() {
                return crypto.tls().get_selected_alpn().value_or(view::rvec{});
            }
        };
    }  // namespace fnet::quic::context
}  // namespace futils

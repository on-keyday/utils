/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/packet_handler.h>
#include <dnet/quic/internal/frame_handler.h>
#include <dnet/quic/packet/packet_util.h>
#include <dnet/quic/frame/frame_util.h>
#include <dnet/quic/frame/vframe.h>
#include <dnet/quic/crypto/crypto.h>

namespace utils {
    namespace dnet {
        namespace quic::handler {

            error::Error handle_frames(QUICContexts* q, ByteLen raw_frames, QPacketNumber packet_number, PacketState state) {
                bool should_gen_ack = false;
                error::Error err;
                frame::parse_frames(raw_frames, [&](frame::Frame& f, bool enc_err) {
                    auto typ = f.type.type_detail();
                    if (enc_err) {
                        err = QUICError{
                            .msg = "frame decode error",
                            .transport_error = TransportError::FRAME_ENCODING_ERROR,
                            .packet_type = state.type,
                            .frame_type = typ,
                        };
                        should_gen_ack = false;
                        return false;
                    }
                    // rfc 9000 12.4. Frames and Frame Types
                    // An endpoint MUST treat receipt of a frame in a packet type
                    // that is not permitted as a connection error of type PROTOCOL_VIOLATION.
                    if (!state.is_ok(f.type.type_detail())) {
                        err = QUICError{
                            .msg = "not allowed frame type for packet type",
                            .transport_error = TransportError::PROTOCOL_VIOLATION,
                            .packet_type = state.type,
                            .frame_type = typ,
                        };
                        should_gen_ack = false;
                        return false;
                    }
                    should_gen_ack = should_gen_ack || is_ACKEliciting(typ);
                    if (typ == FrameType::PING) {
                        return true;
                    }
                    else if (auto a = frame::frame_cast<frame::ACKFrame>(&f)) {
                        err = on_ack(q, *a, state.space);
                    }
                    else if (auto c = frame::frame_cast<frame::CryptoFrame>(&f)) {
                        err = on_crypto(q, *c, state.enc_level);
                    }
                    else if (auto c = frame::frame_cast<frame::ConnectionCloseFrame>(&f)) {
                        err = on_connection_close(q, *c);
                    }
                    else if (auto n = frame::frame_cast<frame::NewConnectionIDFrame>(&f)) {
                        err = on_new_connection_id(q, *n);
                    }
                    else if (auto b = frame::frame_cast<frame::StreamsBlockedFrame>(&f)) {
                        err = on_streams_blocked(q, *b);
                    }
                    else {
                        should_gen_ack = false;
                        err = QUICError{
                            .msg = "unknown/unhandled frame. library bug!!",
                            .frame_type = typ,
                        };
                        return false;
                    }
                    if (err) {
                        should_gen_ack = false;
                        return false;
                    }
                    return true;
                });
                if (should_gen_ack) {
                    auto decoded = q->ackh.decode_packet_number(packet_number, state.space);
                    q->ackh.update_higest_recv_packet(decoded, state.space);
                    q->unacked[int(state.space)].numbers.push_back(decoded);
                }
                return err;
            }

            error::Error common_decrypt_and_handle_frame(QUICContexts* q, std::uint32_t version, PacketState state, ByteLen src, auto& cipher, auto get_pn) {
                auto secret = q->crypto.rsecrets[int(state.enc_level)].secret.unbox();
                crypto::CryptoPacketInfo info = packet::make_cryptoinfo_from_cipher_packet(cipher, src, crypto::cipher_tag_length);
                crypto::Keys keys;
                if (auto err = crypto::decrypt_packet(keys, version, {}, info, secret)) {
                    return QUICError{
                        .msg = "failed to decrypt packet",
                        .transport_error = TransportError::CRYPTO_ERROR,
                        .base = std::move(err),
                    };
                }
                auto plain_text = info.composition();
                return handle_frames(q, info.processed_payload, get_pn(plain_text), state);
            }

            error::Error on_initial_packet(QUICContexts* q, ByteLen src, packet::InitialPacketCipher& packet) {
                auto wp = q->get_tmpbuf();
                auto version = packet.version;
                auto& storage = q->crypto.rsecrets[int(crypto::EncryptionLevel::initial)];
                if (!q->crypto.has_read_secret(crypto::EncryptionLevel::initial)) {
                    auto generated = crypto::make_initial_secret(wp, version, q->params.local.original_dst_connection_id, q->flags.is_server);
                    storage.secret = generated;
                    if (!storage.secret.valid()) {
                        return error::memory_exhausted;
                    }
                }
                return common_decrypt_and_handle_frame(q, packet.version, PacketState{
                                                                              .type = PacketType::Initial,
                                                                              .is_ok = is_InitialPacketOK,
                                                                              .space = ack::PacketNumberSpace::initial,
                                                                              .enc_level = crypto::EncryptionLevel::initial,
                                                                          },
                                                       src, packet, [](auto text) {
                                                           packet::InitialPacketPlain plain;
                                                           plain.parse(text, false);
                                                           return plain.packet_number;
                                                       });
            }

            error::Error on_handshake_packet(QUICContexts* q, ByteLen src, packet::HandshakePacketCipher& packet) {
                if (!q->crypto.has_read_secret(crypto::EncryptionLevel::handshake)) {
                    ParsePendingQUICPackets qp;
                    qp.queued_at = q->ackh.clock.now();
                    qp.type = PacketType::Handshake;
                    qp.cipher_packet = src;
                    if (!qp.cipher_packet.valid()) {
                        return error::memory_exhausted;
                    }
                    q->parse_wait.push_back(std::move(qp));
                    return error::none;  // pending
                }
                return common_decrypt_and_handle_frame(q, packet.version, PacketState{
                                                                              .type = PacketType::Handshake,
                                                                              .is_ok = is_HandshakePacketOK,
                                                                              .space = ack::PacketNumberSpace::handshake,
                                                                              .enc_level = crypto::EncryptionLevel::handshake,
                                                                          },
                                                       src, packet, [](auto text) {
                                                           packet::HandshakePacketPlain plain;
                                                           plain.parse(text, false);
                                                           return plain.packet_number;
                                                       });
            }

            error::Error on_retry_packet(QUICContexts* q, ByteLen src, packet::RetryPacket& packet) {
                packet::RetryPseduoPacket pseduo;
                pseduo.from_retry_packet(q->params.local.original_dst_connection_id, packet);
                auto tmp = q->get_tmpbuf();
                auto pseduo_packet = range_aquire(tmp, [&](WPacket& w) {
                    return pseduo.render(w);
                });
                if (!pseduo_packet.valid()) {
                    // discard packet
                    return error::none;
                }
                crypto::Key<16> tag;
                if (auto err = crypto::generate_retry_integrity_tag(tag, pseduo_packet, packet.version)) {
                    return error::none;
                }
                return error::none;
            }

            error::Error on_0rtt_packet(QUICContexts* q, ByteLen src, packet::ZeroRTTPacketCipher&) {
                return error::none;
            }

            error::Error on_1rtt_packet(QUICContexts* q, ByteLen src, packet::OneRTTPacketCipher& packet) {
                if (!q->crypto.has_read_secret(crypto::EncryptionLevel::application)) {
                    ParsePendingQUICPackets qp;
                    qp.queued_at = q->ackh.clock.now();
                    qp.type = PacketType::OneRTT;
                    qp.cipher_packet = src;
                    if (!qp.cipher_packet.valid()) {
                        return error::memory_exhausted;
                    }
                    q->parse_wait.push_back(std::move(qp));
                    return error::none;
                }
                return common_decrypt_and_handle_frame(q, version_1, PacketState{
                                                                         .type = PacketType::OneRTT,
                                                                         .is_ok = is_OneRTTPacketOK,
                                                                         .space = ack::PacketNumberSpace::application,
                                                                         .enc_level = crypto::EncryptionLevel::application,
                                                                     },
                                                       src, packet, [&](auto text) {
                                                           packet::OneRTTPacketPlain plain;
                                                           plain.parse(
                                                               text, [&](auto, size_t* l) { *l = packet.dstID.len;return true; }, false);
                                                           return plain.packet_number;
                                                       });
            }

            error::Error on_stateless_reset(QUICContexts* q, ByteLen src, packet::StatelessReset&) {
                return error::none;
            }

            error::Error recv_QUIC_packets(QUICContexts* q, ByteLen src) {
                auto is_stateless_reset = [](packet::StatelessReset&) { return false; };
                auto get_dstID_len = [&](ByteLen head, size_t* len) {
                    for (auto& issued : q->srcIDs.connIDs) {
                        if (head.expect(issued.second.id)) {
                            *len = issued.second.id.size();
                            return true;
                        }
                    }
                    *len = 0;
                    return false;
                };
                error::Error err;
                auto cb = [&](PacketType typ, packet::Packet& p, ByteLen src, bool parse_err, bool valid_type) {
                    if (parse_err) {
                        return false;
                    }
                    if (!valid_type) {
                        if (typ == PacketType::LongPacket) {
                            auto lp = static_cast<packet::LongPacketBase&>(p);
                            auto version = lp.version;
                            return false;
                        }
                    }
                    if (typ == PacketType::OneRTT) {
                        err = handler::on_1rtt_packet(q, src, static_cast<packet::OneRTTPacketCipher&>(p));
                    }
                    else if (typ == PacketType::StatelessReset) {
                        err = handler::on_stateless_reset(q, src, static_cast<packet::StatelessReset&>(p));
                    }
                    else if (is_LongPacket(typ)) {
                        auto long_packet = static_cast<packet::LongPacketBase&>(p);
                        size_t seq_id = ~0;
                        for (auto& val : q->srcIDs.connIDs) {
                            if (ByteLen{val.second.id.data(), val.second.id.size()}.equal_to(long_packet.dstID)) {
                                seq_id = val.first;
                                break;
                            }
                        }
                        if (seq_id == ~0) {
                            return true;  // drop packet packet. dstConnID is not match
                        }
                        if (!q->flags.remote_src_recved) {
                            q->dstIDs.add_new(0, {long_packet.srcID.data, long_packet.srcID.len}, nullptr);
                        }
                        else {
                            // TODO(on-keyday): check src ID
                        }
                        switch (typ) {
                            case PacketType::Initial:
                                err = handler::on_initial_packet(q, src, static_cast<packet::InitialPacketCipher&>(p));
                                break;
                            case PacketType::Handshake:
                                err = handler::on_handshake_packet(q, src, static_cast<packet::HandshakePacketCipher&>(p));
                                break;
                            case PacketType::ZeroRTT:
                                err = handler::on_0rtt_packet(q, src, static_cast<packet::ZeroRTTPacketCipher&>(p));
                                break;
                            case PacketType::Retry:
                                err = handler::on_retry_packet(q, src, static_cast<packet::RetryPacket&>(p));
                                break;
                            default:
                                err = QUICError{.msg = "unknown/unhandled long packet. library bug!!"};
                                break;
                        }
                    }
                    else {
                        err = QUICError{
                            .msg = "unknown/unhandhled packet. library bug!!",
                        };
                    }
                    if (err) {
                        return false;
                    }
                    return true;
                };
                packet::parse_packets(src, cb, is_stateless_reset, get_dstID_len);
                return err;
            }
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

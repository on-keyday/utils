/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

/*
#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/packet_handler.h>
#include <dnet/quic/internal/frame_handler.h>
// #include <dnet/quic/packet/packet_util.h>
// #include <dnet/quic/frame/frame_util.h>
// #include <dnet/quic/frame/vframe.h>
#include <dnet/quic/crypto/crypto.h>
#include <core/sequencer.h>
#include <dnet/quic/frame/cast.h>
#include <dnet/quic/frame/parse.h>
#include <dnet/quic/version.h>
#include <dnet/quic/packet/parse.h>

namespace utils {
    namespace dnet {
        namespace quic::handler {

            dnet_dll_implement(error::Error) handle_frames(QUICContexts* q, ByteLen raw_frames, packetnum::WireVal packet_number, PacketState state) {
                bool should_gen_ack = false;
                error::Error err;
                io::reader r{view::rvec(raw_frames.data, raw_frames.len)};
                frame::parse_frames<easy::Vec>(r, [&](frame::Frame& f, bool enc_err) {
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
                    else if (auto a = frame::cast<frame::ACKFrame<easy::Vec>>(&f)) {
                        err = on_ack(q, *a, state.space);
                    }
                    else if (auto c = frame::cast<frame::CryptoFrame>(&f)) {
                        err = on_crypto(q, *c, state.enc_level);
                    }
                    else if (auto c = frame::cast<frame::ConnectionCloseFrame>(&f)) {
                        err = on_connection_close(q, *c);
                    }
                    else if (auto n = frame::cast<frame::NewConnectionIDFrame>(&f)) {
                        err = on_new_connection_id(q, *n);
                    }
                    else if (auto b = frame::cast<frame::StreamsBlockedFrame>(&f)) {
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
                /*auto secret = q->crypto.rsecrets[int(state.enc_level)].secret.unbox();
                packet::CryptoPacket packet;
                packet.head_len = cipher.head_len();
                packet.src = view::wvec(src.data, src.len);
                crypto::Keys keys;
                if (auto err = crypto::make_keys_from_secret(keys, cipher, version, secret)) {
                    return QUICError{
                        .msg = "failed to decrypt packet",
                        .transport_error = TransportError::CRYPTO_ERROR,
                        .base = std::move(err),
                    };
                }
                if (auto err = crypto::decrypt_packet(keys, version, {}, info, secret, )) {
                    return QUICError{
                        .msg = "failed to decrypt packet",
                        .transport_error = TransportError::CRYPTO_ERROR,
                        .base = std::move(err),
                    };
                }
                auto plain_text = info.composition();
                return handle_frames(q, info.processed_payload, get_pn(plain_text), state);*
                return error::none;
            }

            dnet_dll_implement(error::Error) on_initial_packet(QUICContexts* q, ByteLen src, packet::InitialPacketCipher& packet) {
                auto wp = q->get_tmpbuf();
                auto version = packet.version;
                auto& storage = q->crypto.rsecrets[int(crypto::EncryptionLevel::initial)];
                if (!q->crypto.has_read_secret(crypto::EncryptionLevel::initial)) {
                    byte generated[32];
                    auto id = q->params.local.original_dst_connection_id;
                    crypto::make_initial_secret(generated, version, view::rvec(id.data, id.len), q->flags.is_server);
                    storage.secret = ByteLen{generated, 32};
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
                                                           return plain.wire_pn;
                                                       });
            }

            dnet_dll_implement(error::Error) on_handshake_packet(QUICContexts* q, ByteLen src, packet::HandshakePacketCipher& packet) {
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
                                                           return plain.wire_pn;
                                                       });
            }

            dnet_dll_implement(error::Error) on_retry_packet(QUICContexts* q, ByteLen src, packet::RetryPacket& packet) {
                packet::RetryPseduoPacket pseduo;
                auto id = q->params.local.original_dst_connection_id;
                pseduo.from_retry_packet(view::rvec(id.data, id.len), packet);
                flex_storage storage;
                storage.resize(pseduo.len());
                io::writer w{storage.wvec()};
                if (!pseduo.render(w)) {
                    return error::none;  // discard packet
                }
                byte tag[16];
                if (auto err = crypto::generate_retry_integrity_tag(tag, w.written(), packet.version)) {
                    return error::none;
                }
                return error::none;
            }

            dnet_dll_implement(error::Error) on_0rtt_packet(QUICContexts* q, ByteLen src, packet::ZeroRTTPacketCipher&) {
                return error::none;
            }

            dnet_dll_implement(error::Error) on_1rtt_packet(QUICContexts* q, ByteLen src, packet::OneRTTPacketCipher& packet) {
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
                                                               text, [&](auto, size_t* l) { *l = packet.dstID.size();return true; }, false);
                                                           return plain.wire_pn;
                                                       });
            }

            dnet_dll_implement(error::Error) on_stateless_reset(QUICContexts* q, ByteLen src, packet::StatelessReset&) {
                return error::none;
            }

            dnet_dll_implement(error::Error) recv_QUIC_packets(QUICContexts* q, ByteLen src) {
                /*auto is_stateless_reset = [](packet::StatelessReset&) { return false; };
                auto get_dstID_len = [&](io::reader head, size_t* len) {
                    auto seq = make_cpy_seq(head.remain());
                    for (auto& issued : q->srcIDs.connIDs) {
                        if (seq.match(issued.second.id)) {
                            *len = issued.second.id.size();
                            return true;
                        }
                    }
                    *len = 0;
                    return false;
                };
                error::Error err;
                auto cb = [&](PacketType typ, packet::Packet& p, view::wvec src, bool parse_err, bool valid_type) {
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
                            if (val.second.id == long_packet.dstID) {
                                seq_id = val.first;
                            }
                        }
                        if (seq_id == ~0) {
                            return true;  // drop packet packet. dstConnID is not match
                        }
                        if (!q->flags.remote_src_recved) {
                            q->dstIDs.add_new(0, {long_packet.srcID.data(), long_packet.srcID.size()}, nullptr);
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
                packet::parse_packets(view::wvec(src.data, src.len), cb, is_stateless_reset, get_dstID_len);
                return err;*
                return error::none;
            }
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils
*/
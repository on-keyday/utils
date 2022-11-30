/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/quic_contexts.h>
#include <dnet/quic/packet/packet_util.h>
#include <dnet/quic/frame/frame_make.h>
#include <dnet/quic/crypto/crypto.h>
#include <dnet/quic/internal/gen_packet.h>
#include <dnet/quic/version.h>

namespace utils {
    namespace dnet {
        namespace quic::handler {
            std::tuple<ByteLen, TLSCipher, error::Error> setup_wsecret(QUICContexts* q, crypto::EncryptionLevel level, PacketArg& arg) {
                auto wp = q->get_tmpbuf();
                auto& storage = q->crypto.wsecrets[int(level)];
                if (!q->crypto.has_write_secret(level)) {
                    if (level != crypto::EncryptionLevel::initial) {
                        return {{}, {}, error::block};
                    }
                    auto secret = crypto::make_initial_secret(wp, arg.version, arg.origDstID, !q->flags.is_server);
                    if (!secret.valid()) {
                        return {
                            {},
                            {},
                            QUICError{
                                .msg = "failed to generate initial secret",
                            },
                        };
                    }
                    if (auto err = storage.put_secret(secret)) {
                        if (err == error::memory_exhausted) {
                            return {{}, {}, err};
                        }
                        return {{}, {}, QUICError{
                                            .msg = "failed to ecnrypt initial packet",
                                            .base = std::move(err),
                                        }};
                    }
                }
                return {storage.secret.unbox(), storage.cipher, error::none};
            }

            error::Error encrypt_and_enqueue(QUICContexts* q, PacketType type, PacketArg& arg, const TLSCipher& cipher, crypto::CryptoPacketInfo& info, ByteLen secret, size_t packet_number) {
                auto plain = info.composition();
                if (!plain.valid()) {
                    return QUICError{.msg = "failed to generate initial packet"};
                }
                BoxByteLen plain_data = plain;  // boxing
                if (!plain_data.valid()) {
                    return error::memory_exhausted;
                }
                crypto::Keys keys;
                auto r = helper::defer([&]() {
                    clear_memory(keys.resource, sizeof(keys.resource));
                });
                if (auto err = crypto::encrypt_packet(keys, arg.version, cipher, info, secret)) {
                    return QUICError{
                        .msg = "failed to encrypt initial packet",
                        .transport_error = TransportError::CRYPTO_ERROR,
                        .base = std::move(err),
                    };
                }
                r.execute();
                BoxByteLen cipher_data = info.composition();
                if (!cipher_data.valid()) {
                    return error::memory_exhausted;
                }
                return q->quic_packets.enqueue_cipher(
                    std::move(plain_data), std::move(cipher_data),
                    type, ack::PacketNumber(packet_number), true, true);
            }

            error::Error generate_initial_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload) {
                auto [secret, _, err] = setup_wsecret(q, crypto::EncryptionLevel::initial, arg);
                if (err) {
                    return err;
                }
                auto pn = q->ackh.next_packet_number(ack::PacketNumberSpace::initial);
                auto [initial, padding_len] = packet::make_initial_plain(
                    pn.first, arg.version,
                    arg.dstID, arg.srcID, {nullptr, arg.payload_len},
                    arg.token, crypto::cipher_tag_length, arg.reqlen);
                auto wp = q->get_tmpbuf();
                auto info = packet::make_cryptoinfo_from_plain_packet(wp, initial, crypto::cipher_tag_length, packet::PayloadLevel::not_render, [&, padding_len = padding_len](WPacket& w) {
                    gen_payload(wp, padding_len + arg.payload_len);
                    w.add(0, crypto::cipher_tag_length);
                    return true;
                });
                err = encrypt_and_enqueue(q, PacketType::Initial, arg, {}, info, secret, pn.second);
                if (err) {
                    return err;
                }
                q->ackh.consume_packet_number(ack::PacketNumberSpace::initial);
                return error::none;
            }

            error::Error generate_handshake_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload) {
                auto [secret, cipher, err] = setup_wsecret(q, crypto::EncryptionLevel::handshake, arg);
                if (err) {
                    return err;
                }
                auto pn = q->ackh.next_packet_number(ack::PacketNumberSpace::handshake);
                auto [handshake, padding_len] = packet::make_handshake_plain(
                    pn.first, arg.version, arg.dstID, arg.srcID,
                    {nullptr, arg.payload_len}, crypto::cipher_tag_length, arg.reqlen);
                auto wp = q->get_tmpbuf();
                auto info = packet::make_cryptoinfo_from_plain_packet(wp, handshake, crypto::cipher_tag_length, packet::PayloadLevel::not_render, [&, padding_len = padding_len](WPacket& w) {
                    gen_payload(w, padding_len + arg.payload_len);
                    w.add(0, crypto::cipher_tag_length);
                    return true;
                });
                err = encrypt_and_enqueue(q, PacketType::Handshake, arg, cipher, info, secret, pn.second);
                if (err) {
                    return err;
                }
                q->ackh.consume_packet_number(ack::PacketNumberSpace::handshake);
                return error::none;
            }

            error::Error generate_onertt_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload) {
                auto [secret, cipher, err] = setup_wsecret(q, crypto::EncryptionLevel::application, arg);
                if (err) {
                    return err;
                }
                auto pn = q->ackh.next_packet_number(ack::PacketNumberSpace::application);
                auto [onertt, padding_len] = packet::make_onertt_plain(
                    pn.first, arg.version,
                    arg.spin, arg.key_phase, arg.dstID, {nullptr, arg.payload_len}, crypto::cipher_tag_length,
                    arg.reqlen);
                auto wp = q->get_tmpbuf();
                auto info = packet::make_cryptoinfo_from_plain_packet(wp, onertt, crypto::cipher_tag_length, packet::PayloadLevel::not_render, [&, padding_len = padding_len](WPacket& w) {
                    gen_payload(w, arg.payload_len + padding_len);
                    w.add(0, crypto::cipher_tag_length);
                    return true;
                });
                err = encrypt_and_enqueue(q, PacketType::OneRTT, arg, cipher, info, secret, pn.second);
                if (err) {
                    return err;
                }
                q->ackh.consume_packet_number(ack::PacketNumberSpace::application);
                return error::none;
            }

        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/quic_contexts.h>
#include <dnet/quic/packet/packet_util.h>
// #include <dnet/quic/frame/frame_make.h>
#include <dnet/quic/crypto/crypto.h>
#include <dnet/quic/internal/gen_packet.h>
#include <dnet/quic/version.h>
#include <dnet/quic/packet/packet_frame.h>
#include <dnet/quic/internal/gen_frames.h>

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

            error::Error encrypt_and_enqueue(QUICContexts* q, PacketType type, PacketArg& arg, const TLSCipher& cipher,
                                             crypto::CryptoPacketInfo& info, ByteLen secret, size_t packet_number) {
                auto plain = info.composition();
                if (!plain.valid()) {
                    return QUICError{.msg = "failed to generate packet"};
                }
                BoxByteLen plain_data = plain;  // boxing
                if (!plain_data.valid()) {
                    return error::memory_exhausted;
                }
                crypto::Keys keys;
                auto r = helper::defer([&]() {
                    clear_memory(keys.resource, sizeof(keys.resource));
                });
                if (auto err = crypto::encrypt_packet(keys, arg.version, cipher, info, secret, packet_number)) {
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
                    type, ack::PacketNumber(packet_number), arg.ack_eliciting, arg.in_flight);
            }

            dnet_dll_implement(error::Error) generate_initial_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload) {
                auto [secret, _, err] = setup_wsecret(q, crypto::EncryptionLevel::initial, arg);
                if (err) {
                    return err;
                }
                auto pn = q->ackh.next_packet_number(ack::PacketNumberSpace::initial);
                auto [initial, padding_len] = packet::make_initial_plain(
                    pn.first, arg.version,
                    arg.dstID, arg.srcID, arg.token,
                    {nullptr, arg.payload_len}, crypto::authentication_tag_length, arg.reqlen);
                auto wp = q->get_tmpbuf();
                error::Error oerr;
                auto info = packet::make_cryptoinfo_from_plain_packet(wp, initial, crypto::authentication_tag_length, packet::PayloadLevel::not_render, [&, padding_len = padding_len](WPacket& w) {
                    oerr = gen_payload(arg, wp, padding_len + arg.payload_len, pn.second);
                    if (oerr) {
                        return false;
                    }
                    w.add(0, crypto::authentication_tag_length);
                    return true;
                });
                if (oerr) {
                    return oerr;
                }
                err = encrypt_and_enqueue(q, PacketType::Initial, arg, {}, info, secret, pn.second);
                if (err) {
                    return err;
                }
                q->ackh.consume_packet_number(ack::PacketNumberSpace::initial);
                return error::none;
            }

            dnet_dll_implement(error::Error) generate_handshake_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload) {
                auto [secret, cipher, err] = setup_wsecret(q, crypto::EncryptionLevel::handshake, arg);
                if (err) {
                    return err;
                }
                auto pn = q->ackh.next_packet_number(ack::PacketNumberSpace::handshake);
                auto [handshake, padding_len] = packet::make_handshake_plain(
                    pn.first, arg.version, arg.dstID, arg.srcID,
                    {nullptr, arg.payload_len}, crypto::authentication_tag_length, arg.reqlen);
                auto wp = q->get_tmpbuf();
                error::Error oerr;
                auto info = packet::make_cryptoinfo_from_plain_packet(wp, handshake, crypto::authentication_tag_length, packet::PayloadLevel::not_render, [&, padding_len = padding_len](WPacket& w) {
                    oerr = gen_payload(arg, w, padding_len + arg.payload_len, pn.second);
                    if (oerr) {
                        return false;
                    }
                    w.add(0, crypto::authentication_tag_length);
                    return true;
                });
                if (oerr) {
                    return oerr;
                }
                err = encrypt_and_enqueue(q, PacketType::Handshake, arg, cipher, info, secret, pn.second);
                if (err) {
                    return err;
                }
                q->ackh.consume_packet_number(ack::PacketNumberSpace::handshake);
                return error::none;
            }

            dnet_dll_implement(error::Error) generate_onertt_packet(QUICContexts* q, PacketArg arg, GenPayloadCB gen_payload) {
                auto [secret, cipher, err] = setup_wsecret(q, crypto::EncryptionLevel::application, arg);
                if (err) {
                    return err;
                }
                auto pn = q->ackh.next_packet_number(ack::PacketNumberSpace::application);
                auto [onertt, padding_len] = packet::make_onertt_plain(
                    pn.first, arg.version,
                    arg.spin, arg.key_phase, arg.dstID, {nullptr, arg.payload_len}, crypto::authentication_tag_length,
                    arg.reqlen);
                auto wp = q->get_tmpbuf();
                error::Error oerr;
                auto info = packet::make_cryptoinfo_from_plain_packet(wp, onertt, crypto::authentication_tag_length, packet::PayloadLevel::not_render, [&, padding_len = padding_len](WPacket& w) {
                    oerr = gen_payload(arg, w, arg.payload_len + padding_len, pn.second);
                    if (oerr) {
                        return false;
                    }
                    w.add(0, crypto::authentication_tag_length);
                    return true;
                });
                if (oerr) {
                    return oerr;
                }
                err = encrypt_and_enqueue(q, PacketType::OneRTT, arg, cipher, info, secret, pn.second);
                if (err) {
                    return err;
                }
                q->ackh.consume_packet_number(ack::PacketNumberSpace::application);
                return error::none;
            }

            auto make_frame_cb(QUICContexts* q, PacketType pactype, size_t use_frame, size_t offset, auto& frames, size_t payload_len, size_t padding_len) {
                return [=, &frames](PacketArg& arg, WPacket& wp, size_t space, size_t packet_number) -> error::Error {
                    assert(space == payload_len + padding_len);
                    arg.ack_eliciting = false;
                    arg.in_flight = false;
                    wp.add(0, padding_len);  // add padding
                    io::writer w{view::wvec(wp.b.data + wp.offset, wp.b.len - wp.offset)};
                    for (size_t k = offset; k < use_frame; k++) {
                        error::Error gen_err;
                        frame::Frame* fr = frames[k]->frame();
                        /*
                        auto res = frame::vframe_apply_lite(frames[k], [&](auto* ptr) {
                            frtyp = ptr->type();
                            arg.ack_eliciting = arg.ack_eliciting || is_ACKEliciting(frtyp);
                            arg.in_flight = arg.in_flight || is_ByteCounted(frtyp);
                            gen_err = on_frame_render(q, *ptr->frame(), packet_number);
                            if (!gen_err) {
                                return false;
                            }
                            return ptr->frame()->render(wp);
                        });*/
                        auto frtype = fr->type.type_detail();
                        arg.ack_eliciting = arg.ack_eliciting || is_ACKEliciting(frtype);
                        arg.in_flight = arg.in_flight || is_ByteCounted(frtype);
                        if (auto err = on_frame_render(q, *fr, packet_number)) {
                            return err;
                        }
                        if (!fr->render(w)) {
                            return QUICError{
                                .msg = "failed to generate frame",
                                .transport_error = TransportError::FRAME_ENCODING_ERROR,
                                .packet_type = pactype,
                                .frame_type = frtype,
                            };
                        }
                    }
                    wp.offset += w.offset();
                    return error::none;
                };
            }

            static constexpr PacketType typ[3] = {PacketType::Initial, PacketType::Handshake, PacketType::OneRTT};

            static constexpr decltype(generate_initial_packet)* generate[] = {generate_initial_packet, generate_handshake_packet, generate_onertt_packet};

            error::Error generate_coalescing_packet(QUICContexts* q, PacketArg arg) {
                bool has_initial = q->frames[0].size() != 0;
                bool has_handshake = q->frames[1].size() != 0;
                if (!has_initial && !has_handshake) {
                    return error::none;  // nothing to do
                }
                auto limit = q->udp_limit();
                std::pair<QPacketNumber, size_t> pns[] = {
                    q->ackh.next_packet_number(ack::PacketNumberSpace::initial),
                    q->ackh.next_packet_number(ack::PacketNumberSpace::handshake),
                    q->ackh.next_packet_number(ack::PacketNumberSpace::application),
                };
                auto guess = [&](size_t i, size_t used, size_t padding = 0) {
                    return packet::guess_packet_length_with_frames(
                        limit - used, typ[i], arg.dstID.len, arg.srcID.len,
                        pns[i].first, q->frames[i], padding);
                };
                auto gen = [&](size_t i, packet::GuessTuple tuple, size_t padding) {
                    auto count = tuple.count;
                    auto packet_len = tuple.packet_len;
                    auto payload_len = tuple.payload_len;
                    auto cb = make_frame_cb(q, typ[i], count, 0, q->frames[i], payload_len, padding);
                    arg.payload_len = payload_len;
                    arg.reqlen = packet_len;
                    return generate[i](q, arg, cb);
                };
                auto initial_tuple = guess(0, 0);
                if (initial_tuple.limit_reached) {
                    // single initial packet reaches udp datagram limit
                    // sned single
                    return error::none;
                }
                auto& ini_len = initial_tuple.packet_len;
                auto hs_tuple = guess(1, ini_len);
                auto& hs_len = hs_tuple.packet_len;
                if (hs_tuple.limit_reached) {
                    if (ini_len == 0) {
                        // single handshake packet reaches udp datagram limit
                        // send single
                        return error::none;
                    }
                    auto padding = limit - (ini_len + hs_len);
                    auto guess_tuple = guess(1, ini_len, padding);
                    if (guess_tuple.packet_len + ini_len != limit) {
                        // TODO(on-keyday):
                        // should check packet size more?
                        return error::none;
                    }
                    if (auto err = gen(0, initial_tuple, 0)) {
                        return err;
                    }
                    if (auto err = gen(1, guess_tuple, padding)) {
                        return err;
                    }
                    q->frames[0].pop(initial_tuple.count);
                    q->frames[1].pop(guess_tuple.count);
                    return error::none;
                }
                assert(ini_len || hs_len);
                auto app_tuple = guess(2, ini_len + hs_len);
                auto& app_len = app_tuple.packet_len;
                if (app_len == 0 && (hs_len == 0 || ini_len == 0)) {
                    // single packet pattern
                    return error::none;
                }
                auto padding = ini_len ? limit - (ini_len + hs_len + app_len) : 0;
                app_tuple = guess(2, ini_len + hs_len, padding);
                if (ini_len) {
                    if (auto err = gen(0, initial_tuple, 0)) {
                        return err;
                    }
                    q->frames[0].pop(initial_tuple.count);
                }
                if (hs_len) {
                    if (auto err = gen(1, hs_tuple, 0)) {
                        return err;
                    }
                    q->frames[1].pop(hs_tuple.count);
                }
                if (auto err = gen(2, app_tuple, padding)) {
                    return err;
                }
                q->frames[2].pop(app_tuple.count);
                return error::none;
            }

            dnet_dll_implement(error::Error) generate_packets(QUICContexts* q) {
                // get current UDP payload size limit
                auto limit = q->udp_limit();
                if (limit < dgram::initial_datagram_size_limit) {
                    return QUICError{
                        .msg = "current udp limit is under 1200",
                    };
                }

                // get srcID and dstID for packet
                auto srcID = q->srcIDs.connIDs[q->srcIDs.seq_id - 1].get();
                auto dstID = q->dstIDs.get().get();

                // limit UDP payload including initial packet
                bool initial_size_limiter = !q->flags.is_server;

                // set PacketArg common parameter
                PacketArg arg;
                arg.srcID = srcID;
                arg.dstID = dstID;
                arg.origDstID = q->params.local.original_dst_connection_id;
                arg.version = version_1;

                std::pair<QPacketNumber, size_t> pns[3];

                auto update_pns = [&] {
                    pns[0] = q->ackh.next_packet_number(ack::PacketNumberSpace::initial);
                    pns[1] = q->ackh.next_packet_number(ack::PacketNumberSpace::handshake);
                    pns[2] = q->ackh.next_packet_number(ack::PacketNumberSpace::application);
                };

                auto guess = [&](size_t i, size_t padding) {
                    return packet::guess_packet_length_with_frames(
                        limit, typ[i], arg.dstID.len, arg.srcID.len,
                        pns[i].first, q->frames[i], padding);
                };

                auto gen = [&](size_t i, packet::GuessTuple tuple, size_t padding) {
                    auto cb = make_frame_cb(q, typ[i], tuple.count, 0, q->frames[i], tuple.payload_len, padding);
                    arg.payload_len = tuple.payload_len;
                    arg.reqlen = tuple.packet_len;
                    return generate[i](q, arg, cb);
                };

                while (true) {
                    if (auto err = generate_coalescing_packet(q, arg)) {
                        return err;
                    }

                    update_pns();

                    auto i = 0;
                    for (; i < 3; i++) {
                        auto& frames = q->frames[i];
                        if (frames.size() == 0) {
                            continue;  // nothing to deliver
                        }
                        // padding length
                        size_t padding_len = 0;
                        auto tuple = guess(i, 0);
                        if (tuple.count == 0) {
                            continue;  // no way to deliver packet?
                        }
                        if (tuple.payload_len < 20 - pns[i].first.len) {
                            padding_len += (20 - pns[i].first.len) - tuple.payload_len;
                        }
                        if (i == 0 && initial_size_limiter) {
                            padding_len += limit - tuple.packet_len - padding_len;
                        }
                        tuple = guess(i, padding_len);
                        if (auto err = gen(i, tuple, padding_len)) {
                            return err;
                        }
                        break;
                    }
                    if (i == 3) {
                        break;
                    }
                }
                return error::none;
            }

        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

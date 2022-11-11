/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/packet_handler.h>
#include <dnet/quic/internal/frame_handler.h>
#include <dnet/quic/packet_util.h>
#include <dnet/quic/frame/frame_util.h>

namespace utils {
    namespace dnet {
        namespace quic::handler {
            bool on_initial_packet(QUICContexts* q, packet::InitialPacketCipher& packet) {
                auto [wp, _] = make_tmpbuf(3600);
                auto info = packet::make_cryptoinfo_from_cipher_packet(packet, crypto::cipher_tag_length);
                auto version = packet.version.as<std::uint32_t>();
                auto secret = crypto::make_initial_secret(wp, version, q->params.original_dst_connection_id, false);
                crypto::Keys keys;
                if (!crypto::decrypt_packet(keys, version, {}, info, secret)) {
                    return false;
                }
                packet::InitialPacketPlain plain;
                auto plain_text = info.composition();
                plain.parse(plain_text, crypto::cipher_tag_length);
                bool should_gen_ack = false;
                auto res = frame::parse_frames(info.processed_payload, [&](frame::Frame& f, bool err) {
                    auto typ = f.type.type_detail();
                    if (err) {
                        q->transport_error(TransportError::FRAME_ENCODING_ERROR, typ, "frame decode error");
                        should_gen_ack = false;
                        return false;
                    }
                    // rfc 9000 12.4. Frames and Frame Types
                    // An endpoint MUST treat receipt of a frame in a packet type
                    // that is not permitted as a connection error of type PROTOCOL_VIOLATION.
                    if (!is_InitialPacketOK(f.type.type_detail())) {
                        q->transport_error(TransportError::PROTOCOL_VIOLATION, typ, "not allowed frame type on initial packet");
                        should_gen_ack = false;
                        return false;
                    }
                    should_gen_ack = should_gen_ack && is_ACKEliciting(typ);
                    if (auto a = frame::frame_cast<frame::ACKFrame>(&f)) {
                        return on_ack(q, *a, ack::PacketNumberSpace::initial);
                    }
                    if (auto c = frame::frame_cast<frame::CryptoFrame>(&f)) {
                        return on_crypto(q, *c, crypto::EncryptionLevel::initial);
                    }
                    if (auto c = frame::frame_cast<frame::ConnectionCloseFrame>(&f)) {
                        return on_connection_close(q, *c);
                    }
                    if (typ == FrameType::PING) {
                        return true;
                    }
                    should_gen_ack = false;
                    q->internal_error("unknown frame");
                    return false;
                });
                if (should_gen_ack) {
                    std::uint32_t pn = 0;
                    auto pn_len = plain.flags.packet_number_length();
                    plain.packet_number.packet_number(pn, pn_len);
                    // q->ackh.decode_packet_number(ack::PacketNumberSpace::initial, pn, pn_len * 8);
                }
                return res;
            }
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

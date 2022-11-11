/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/quic_contexts.h>
#include <dnet/quic/frame/frame_make.h>
#include <dnet/quic/packet_util.h>
#include <dnet/quic/frame/frame_util.h>
#include <dnet/quic/internal/packet_handler.h>
#include <malloc.h>

namespace utils {
    namespace dnet {
        namespace quic {
            std::pair<WPacket, BoxByteLen> make_tmpbuf(size_t size) {
                BoxByteLen ub{size};
                WPacket wp{{ub.data(), ub.len()}};
                return {wp, std::move(ub)};
            }

            bool start_tls_handshake(WPacket& wp, QUICContexts* p) {
                if (!p->params.original_dst_connection_id.enough(8)) {
                    p->initialDstID = p->srcIDs.genrandom(20);
                    p->params.original_dst_connection_id = {p->initialDstID.data(), 20};
                }
                auto tparam = p->params.to_transport_param(wp);
                ubytes tp;
                for (auto& param : tparam) {
                    if (is_defined_both_set_allowed(param.id.qvarint())) {
                        if (!param.data.valid()) {
                            continue;
                        }
                        auto plen = param.param_len();
                        auto oldlen = tp.size();
                        tp.resize(oldlen + plen);
                        WPacket tmp{tp.data() + oldlen, plen};
                        if (!param.render(tmp)) {
                            p->internal_error("failed to generate transport parameter");
                            return false;
                        }
                    }
                }
                if (!p->tls.set_quic_transport_params(tp.data(), tp.size())) {
                    p->internal_error("failed to set transport parameter");
                    return false;
                }
                if (!p->tls.connect()) {
                    if (!p->tls.block()) {
                        if (p->state == QState::tls_alert) {
                            p->transport_error(to_CRYPTO_ERROR(p->tls_alert), FrameType::CRYPTO, "TLS message error");
                            return false;
                        }
                        p->internal_error("unknwon error on tls.connect.");
                        p->tls.get_errors([&](const char* data, size_t len) {
                            p->debug_msg.push_back(' ');
                            p->debug_msg.append(data, len);
                            return 1;
                        });
                        return false;
                    }
                }
                return true;
            }

            void start_initial_handshake(QUICContexts* p) {
                auto [wp, _] = make_tmpbuf(3600);
                if (!start_tls_handshake(wp, p)) {
                    return;
                }
                wp.reset_offset();
                auto& data = p->hsdata[0].data;
                auto crypto_data = ByteLen{data.data(), data.size()};
                auto srcID = p->srcIDs.issue(20);
                auto crypto = frame::make_crypto(wp, 0, crypto_data);
                auto crlen = wp.acquire(crypto.frame_len());
                WPacket tmp{crlen};
                crypto.render(tmp);
                auto initial = packet::make_initial_plain(
                    wp, {0, 1}, 1,
                    {srcID.data(), srcID.size()}, p->params.original_dst_connection_id, {},
                    crlen, crypto::cipher_tag_length, 1200);
                auto ofs = wp.offset;
                auto packet = packet::make_cryptoinfo(wp, initial, crypto::cipher_tag_length);
                auto plain = packet.composition();
                if (!plain.valid()) {
                    p->internal_error("failed to generate initial packet");
                    return;
                }
                BoxByteLen plain_data = plain;
                if (!plain_data.valid()) {
                    p->internal_error("memory exhausted");
                    return;
                }
                crypto::Keys keys;
                auto secret = crypto::make_initial_secret(wp, 1, initial.dstID, true);
                if (!crypto::encrypt_packet(keys, 1, {}, packet, secret)) {
                    p->internal_error("failed to encrypt initial packet");
                    return;
                }
                auto dat = QUICPackets{};
                dat.cipher = packet.composition();
                if (!dat.cipher.valid()) {
                    p->internal_error("memory exhausted");
                    return;
                }
                dat.sent.sent_plain = std::move(plain_data);
                dat.sent.sent_bytes = dat.cipher.len();
                dat.sent.ack_eliciting = true;
                dat.sent.in_flight = true;
                dat.sent.packet_number = ack::PacketNumber{0};
                dat.pn_space = ack::PacketNumberSpace::initial;
                p->quic_packets.push_back(std::move(dat));
                p->state = QState::receving_peer_handshake_packets;
            }

            quic_wait_state receiving_peer_handshake_packets(QUICContexts* q) {
                if (!q->datagrams.size()) {
                    return quic_wait_state::block;
                }
                auto dgram = std::move(q->datagrams.front());
                auto unboxed = dgram.b.unbox();
                auto is_stateless_reset = [](packet::StatelessReset&) { return false; };
                auto get_dstID_len = [&](ByteLen head, size_t* len) {
                    for (auto& issued : q->srcIDs.connIDs) {
                        if (head.expect(issued)) {
                            *len = issued.size();
                            return true;
                        }
                    }
                    *len = 0;
                    return false;
                };
                auto cb = [&](PacketType typ, packet::Packet& p, bool err, bool valid_type) {
                    if (err) {
                        return false;
                    }
                    if (!valid_type) {
                        if (typ == PacketType::LongPacket) {
                            auto lp = static_cast<packet::LongPacketBase&>(p);
                            auto version = lp.version.as<std::uint32_t>();
                        }
                    }
                    if (typ == PacketType::Initial) {
                        return handler::on_initial_packet(q, static_cast<packet::InitialPacketCipher&>(p));
                    }
                    q->internal_error("unknown packet");
                    return false;
                };
                packet::parse_packets(unboxed, cb, is_stateless_reset, get_dstID_len);
                return quic_wait_state::next;
            }

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

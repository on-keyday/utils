/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/quic.h>
#include <helper/defer.h>
#include <dnet/quic/internal/quic_contexts.h>
#include <dnet/quic/flow/initial.h>

namespace utils {
    namespace dnet {
        namespace quic {

            QUIC::~QUIC() {
                if (p) {
                    delete_with_global_heap(p, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(QUICContexts)));
                }
            }

            bool QUIC::provide_udp_datagram(const void* data, size_t size) {
                if (!p) {
                    return false;
                };
                auto boxed = BoxByteLen(ByteLen{(byte*)data, size});
                if (boxed.unbox().len != size) {
                    return false;
                }
                p->datagrams.push_back(UDPDatagram{std::move(boxed)});
                return true;
            }

            bool QUIC::receive_quic_packet(void* data, size_t size, size_t* red) {
                auto set_red = [&](auto v) {
                    if (red) {
                        *red = v;
                    }
                };
                if (!p) {
                    set_red(0);
                    return false;
                }
                if (!p->quic_packets.size()) {
                    set_red(0);
                    return false;
                }
                auto& fr = p->quic_packets.front();
                auto unboxed = fr.b.unbox();
                set_red(unboxed.len);
                if (!data || unboxed.len > size) {
                    return false;
                }
                memcpy(data, unboxed.data, unboxed.len);
                p->ackh.on_packet_sent(fr.packet_number, fr.pn_space, fr.ack_eliciting, fr.in_flight, unboxed);
                p->quic_packets.pop_front();
                return true;
            }

            int secrets(QUICContexts* c, MethodArgs args) {
                if (args.type == ArgType::secret) {
                    if (args.level == EncryptionLevel::initial) {
                        return 0;
                    }
                    auto index = int(args.level) - 1;
                    auto& wsecret = c->wsecrets[index];
                    auto& rsecret = c->rsecrets[index];
                    wsecret.b = ByteLen{(byte*)args.write_secret, args.secret_len};
                    rsecret.b = ByteLen{(byte*)args.read_secret, args.secret_len};
                    if (!wsecret.b.valid() || !rsecret.b.valid()) {
                        return 0;
                    }
                    wsecret.cipher = c->tls.get_cipher();
                    rsecret.cipher = c->tls.get_cipher();
                    return 1;
                }
                if (args.type == ArgType::wsecret) {
                    if (args.level == EncryptionLevel::initial) {
                        return 0;
                    }
                    auto& wsecret = c->wsecrets[int(args.level) - 1];
                    wsecret.b = ByteLen{(byte*)args.write_secret, args.secret_len};
                    if (!wsecret.b.valid()) {
                        return 0;
                    }
                    wsecret.cipher = args.cipher;
                    return 1;
                }
                if (args.type == ArgType::rsecret) {
                    if (args.level == EncryptionLevel::initial) {
                        return 0;
                    }
                    auto& rsecret = c->rsecrets[int(args.level) - 1];
                    rsecret.b = ByteLen{(byte*)args.read_secret, args.secret_len};
                    if (!rsecret.b.valid()) {
                        return 0;
                    }
                    rsecret.cipher = args.cipher;
                    return 1;
                }
                return 0;
            }

            int qtls_(QUICContexts* c, MethodArgs args) {
                if (secrets(c, args)) {
                    return 1;
                }
                if (args.type == ArgType::handshake_data) {
                    c->hsdata[int(args.level)].data.append(args.data, args.len);
                    return 1;
                }
                if (args.type == ArgType::alert) {
                    c->tls_alert = args.alert;
                    c->state = QState::tls_alert;
                    return 1;
                }
                if (args.type == ArgType::flush) {
                    c->flush_called = true;
                    return 1;
                }
                return 0;
            }

            dnet_dll_implement(QUIC) make_quic(TLS& tls) {
                if (!tls.has_sslctx() || tls.has_ssl()) {
                    return {};
                }
                auto p = new_from_global_heap<QUICContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(QUICContexts)));
                auto r = helper::defer([&] {
                    delete_with_global_heap(p, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(QUICContexts)));
                });
                if (!tls.make_quic(qtls_, p)) {
                    return {};
                }
                p->tls = std::move(tls);
                p->ackh.init();
                r.cancel();
                return QUIC{p};
            }

            std::pair<WPacket, ubytes> make_tmpbuf(size_t size) {
                ubytes ub;
                ub.resize(size);
                WPacket wp{{ub.data(), ub.size()}};
                return {wp, ub};
            }

            void do_start_connect(QUICContexts* p) {
                auto [wp, _] = make_tmpbuf(3000);
                auto tparam = p->params.to_transport_param(wp);
                ubytes tp;
                for (auto& param : tparam) {
                    if (is_defined_both_set_allowed(param.id.qvarint())) {
                        auto plen = param.param_len();
                        auto oldlen = tp.size();
                        tp.resize(oldlen + plen);
                        WPacket tmp{tp.data() + oldlen, plen};
                        if (!param.render(tmp)) {
                            p->internal_error("failed to generate transport parameter");
                            return;
                        }
                    }
                }
                if (!p->tls.set_quic_transport_params(tp.data(), tp.size())) {
                    p->internal_error("failed to set transport parameter");
                    return;
                }
                if (!p->tls.connect()) {
                    if (!p->tls.block()) {
                        if (p->state == QState::tls_alert) {
                            p->debug_msg = "TLS message error";
                            return;
                        }
                        p->internal_error("unknwon error on tls.connect");
                        return;
                    }
                }
                p->state = QState::sending_initial_packet;
            }

            void do_send_initial_packet(QUICContexts* p) {
                auto [wp, _] = make_tmpbuf(3000);
                auto& data = p->hsdata[0].data;
                auto crypto_data = ByteLen{data.data(), data.size()};
                auto srcID = p->connIDs.issue(20);
                auto inipacket = flow::make_first_flight(wp, 0, 1, crypto_data, p->params.original_dst_connection_id,
                                                         {srcID.data(), srcID.size()}, {});
                auto ofs = wp.offset;
                auto packet = flow::makePacketInfo(wp, inipacket, true);
                if (!encrypt_initial_packet(packet, true)) {
                    p->internal_error("failed to encrypt initial packet");
                    return;
                }
                auto packet_data = wp.b.data + ofs;
                auto packet_len = wp.offset - ofs;
                auto dat = QUICPackets{};
                dat.b = ByteLen{packet_data, packet_len};
                if (!dat.b.valid()) {
                    p->internal_error("memory exhausted");
                    return;
                }
                p->quic_packets.push_back(std::move(dat));
            }

            quic_wait_state receiving_peer_handshake_packets(QUICContexts* q) {
                if (!q->datagrams.size()) {
                    return quic_wait_state::block;
                }
                auto dgram = std::move(q->datagrams.front());
                auto unboxed = dgram.b.unbox();
            }

            bool QUIC::connect() {
                if (!p) {
                    return false;
                }
                while (true) {
                    if (p->state == QState::start) {
                        do_start_connect(p);
                        continue;
                    }
                    if (p->state == QState::sending_initial_packet) {
                        do_send_initial_packet(p);
                        continue;
                    }
                    if (p->state == QState::receving_peer_handshake_packets) {
                        if (auto r = receiving_peer_handshake_packets(p);
                            r == quic_wait_state::block) {
                            return false;
                        }
                        continue;
                    }
                    if (p->state == QState::internal_error) {
                        if (p->has_established) {
                            // TODO(on-keyday): implement push to server
                        }
                        p->state = QState::fatal;
                    }
                    break;
                }
                return false;
            }
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

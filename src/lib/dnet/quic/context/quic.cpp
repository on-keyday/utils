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
#include <dnet/quic/packet_util.h>
#include <dnet/quic/frame/frame_make.h>
#include <dnet/dll/glheap.h>

namespace utils {
    namespace dnet {
        namespace quic {

            QUIC::~QUIC() {
                if (p) {
                    delete_with_global_heap(p, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(QUICContexts), alignof(QUICContexts)));
                }
            }

            bool QUICTLS::set_hostname(const char* name, bool verify) {
                if (!tls) {
                    return false;
                }
                return tls->set_hostname(name, verify);
            }

            bool QUICTLS::set_alpn(const char* name, size_t len) {
                if (!tls) {
                    return false;
                }
                return tls->set_alpn(name, len);
            }

            bool QUICTLS::set_client_cert_file(const char* cert) {
                if (!tls) {
                    return false;
                }
                return tls->set_client_cert_file(cert);
            }

            bool QUICTLS::set_verify(int mode, int (*verify_callback)(int, void*)) {
                if (!tls) {
                    return false;
                }
                return tls->set_verify(mode, verify_callback);
            }

            bool QUIC::provide_udp_datagram(const void* data, size_t size) {
                if (!p) {
                    return false;
                };
                auto boxed = BoxByteLen(ConstByteLen{static_cast<const byte*>(data), size});
                if (boxed.unbox().len != size) {
                    return false;
                }
                p->datagrams.push_back(UDPDatagram{std::move(boxed)});
                auto err = p->ackh.on_datagram_recived();
                if (err != TransportError::NO_ERROR) {
                    p->transport_error(err, FrameType::PADDING, "ackh.on_datagram_recived failed");
                }
                return true;
            }

            bool QUIC::receive_quic_packets(void* data, size_t size, size_t* red, size_t* npacket) {
                size_t pcount = 0;
                size_t offset = 0;
                auto set_red = [&](auto v, auto np) {
                    offset += v;
                    pcount += np;
                    if (red) {
                        *red = offset;
                    }
                    if (npacket) {
                        *npacket = pcount;
                    }
                };
                if (!p || !data) {
                    set_red(0, 0);
                    return false;
                }
                set_red(0, 0);
                auto dp = static_cast<byte*>(data);
                while (true) {
                    if (!p->quic_packets.size()) {
                        return pcount != 0;
                    }
                    auto& fr = p->quic_packets.front();
                    auto unboxed = fr.cipher.unbox();
                    if (offset + unboxed.len > size) {
                        return pcount != 0;
                    }
                    memcpy(dp + offset, unboxed.data, unboxed.len);
                    offset += unboxed.len;
                    set_red(unboxed.len, 1);
                    auto err = p->ackh.on_packet_sent(fr.pn_space, std::move(fr.sent));
                    p->quic_packets.pop_front();
                    if (err != TransportError::NO_ERROR) {
                        p->transport_error(err, {}, "ackh.on_packet_sent failed");
                        return false;
                    }
                }
                return true;
            }

            size_t QUIC::quic_packet_len(size_t npacket) const {
                if (!p) {
                    return 0;
                }
                size_t len = 0;
                for (size_t i = 0; i < npacket; i++) {
                    if (i >= p->quic_packets.size()) {
                        break;
                    }
                    len += p->quic_packets[i].cipher.len();
                }
                return len;
            }

            QUICTLS QUIC::tls() {
                if (!p) {
                    return {};
                }
                return QUICTLS{&p->tls};
            }

            int secrets(QUICContexts* c, MethodArgs args) {
                if (args.type == crypto::ArgType::secret) {
                    if (args.level == crypto::EncryptionLevel::initial) {
                        return 0;
                    }
                    auto index = int(args.level);
                    auto& wsecret = c->wsecrets[index];
                    auto& rsecret = c->rsecrets[index];
                    wsecret.b = ConstByteLen{args.write_secret, args.secret_len};
                    rsecret.b = ConstByteLen{args.read_secret, args.secret_len};
                    if (!wsecret.b.valid() || !rsecret.b.valid()) {
                        return 0;
                    }
                    wsecret.cipher = c->tls.get_cipher();
                    rsecret.cipher = c->tls.get_cipher();
                    return 1;
                }
                if (args.type == crypto::ArgType::wsecret) {
                    if (args.level == crypto::EncryptionLevel::initial) {
                        return 0;
                    }
                    auto& wsecret = c->wsecrets[int(args.level) - 1];
                    wsecret.b = ConstByteLen{args.write_secret, args.secret_len};
                    if (!wsecret.b.valid()) {
                        return 0;
                    }
                    wsecret.cipher = args.cipher;
                    return 1;
                }
                if (args.type == crypto::ArgType::rsecret) {
                    if (args.level == crypto::EncryptionLevel::initial) {
                        return 0;
                    }
                    auto& rsecret = c->rsecrets[int(args.level) - 1];
                    rsecret.b = ConstByteLen{args.read_secret, args.secret_len};
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
                if (args.type == crypto::ArgType::handshake_data) {
                    c->hsdata[int(args.level)].data.append(args.data, args.len);
                    return 1;
                }
                if (args.type == crypto::ArgType::alert) {
                    c->tls_alert = args.alert;
                    c->state = QState::tls_alert;
                    return 1;
                }
                if (args.type == crypto::ArgType::flush) {
                    c->flush_called = true;
                    return 1;
                }
                return 0;
            }

            bool apply_config(QUICContexts* p, const Config* c) {
                // ACK handler configs
                if (!c->ack_handler.now || c->ack_handler.msec == 0 ||
                    c->ack_handler.time_threshold.den == 0) {
                    return false;
                }
                p->ackh.clock.now_fn = c->ack_handler.now;
                p->ackh.clock.ctx = c->ack_handler.now_ctx;
                p->ackh.clock.granularity = c->ack_handler.msec;
                p->ackh.params.time_threshold_ratio.num = c->ack_handler.time_threshold.num;
                p->ackh.params.time_threshold_ratio.den = c->ack_handler.time_threshold.den;
                p->ackh.params.packet_order_threshold = c->ack_handler.packet_threshold;
                p->ackh.rtt.set_inirtt(c->ack_handler.initialRTT);
                // Transport Parameter
                p->params = c->transport_parameter.params;
                return true;
            }

            dnet_dll_implement(QUIC) make_quic(TLS& tls, const Config& config) {
                if (!tls.has_sslctx() || tls.has_ssl()) {
                    return {};
                }
                auto p = new_from_global_heap<QUICContexts>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(QUICContexts), alignof(QUICContexts)));
                auto r = helper::defer([&] {
                    delete_with_global_heap(p, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(QUICContexts), alignof(QUICContexts)));
                });
                if (!apply_config(p, &config)) {
                    return {};
                }
                p->tls = std::move(tls);
                if (!p->tls.make_quic(qtls_, p)) {
                    tls = std::move(p->tls);
                    return {};
                }
                r.cancel();
                return QUIC{p};
            }

            bool QUIC::block() const {
                if (!p) {
                    return false;
                }
                return p->block;
            }

            bool QUIC::connect() {
                if (!p) {
                    return false;
                }
                p->block = false;
                while (true) {
                    if (p->state == QState::start) {
                        start_initial_handshake(p);
                        continue;
                    }
                    if (p->state == QState::receving_peer_handshake_packets) {
                        auto res = receiving_peer_handshake_packets(p);
                        if (res == quic_wait_state::block) {
                            p->block = true;
                            return false;
                        }
                        continue;
                    }
                    if (p->state == QState::on_error) {
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

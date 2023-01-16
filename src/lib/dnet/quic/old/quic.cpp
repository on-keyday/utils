/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

/*
#include <dnet/dll/dllcpp.h>
#include <dnet/quic/quic.h>
#include <helper/defer.h>
#include <dnet/quic/internal/quic_contexts.h>
// #include <dnet/quic/packet/packet_util.h>
#include <dnet/quic/frame/crypto.h>
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
                return tls->set_hostname(name, verify).is_noerr();
            }

            bool QUICTLS::set_alpn(const char* name, size_t len) {
                if (!tls) {
                    return false;
                }
                return tls->set_alpn(name, len).is_noerr();
            }

            bool QUICTLS::set_client_cert_file(const char* cert) {
                if (!tls) {
                    return false;
                }
                return tls->set_client_cert_file(cert).is_noerr();
            }

            bool QUICTLS::set_verify(int mode, int (*verify_callback)(int, void*)) {
                if (!tls) {
                    return false;
                }
                return tls->set_verify(mode, verify_callback).is_noerr();
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
                if (err) {
                    p->errs.err = std::move(err);
                }
                return true;
            }

            void set_errors(QUICContexts* q, error::Error& err) {
                if (err == error::block) {
                    return;
                }
                q->errs.err = err;
                q->errs.qerr = q->errs.err.as<quic::QUICError>();
                q->state = QState::on_error;
            }

            bool QUIC::receive_quic_packet(PacketType& typ, void* data, size_t size, size_t& red) {
                if (!p || p->quic_packets.size() == 0) {
                    red = 0;
                    return false;
                }
                auto& qp = p->quic_packets.front();
                auto len = qp.sending_packet.len();
                red = len;
                typ = qp.sent.type;
                if (size < len || !data) {
                    return false;
                }
                if (qp.pn_space != ack::PacketNumberSpace::no_space) {
                    if (auto err = p->ackh.on_packet_sent(qp.pn_space, std::move(qp.sent))) {
                        set_errors(p, err);
                        return false;
                    }
                }
                auto to = (byte*)data;
                auto from = qp.sending_packet.data();
                for (size_t i = 0; i < len; i++) {
                    to[i] = from[i];
                }
                p->quic_packets.pop_front();
                return true;
            }

            bool QUIC::receive_udp_datagram(void* data, size_t size, size_t& recved) {
                recved = 0;
                if (!p || p->quic_packets.size() == 0) {
                    return false;
                }
                size_t limit = p->udp_limit();
                if (size < limit) {
                    recved = limit;
                    return false;
                }
                size_t red = 0;
                auto seq = reinterpret_cast<byte*>(data);
                PacketType typ = PacketType::Unknown;
                for (;;) {
                    if (!receive_quic_packet(typ, seq + recved, limit - recved, red)) {
                        break;
                    }
                    recved += red;
                    if (has_LengthField(typ)) {
                        continue;
                    }
                    break;
                }
                return true;
            }

            QUICTLS QUIC::tls() {
                if (!p) {
                    return {};
                }
                return QUICTLS{&p->crypto.tls};
            }

            int secrets(QUICContexts* c, MethodArgs args) {
                if (args.type == crypto::ArgType::secret) {
                    if (args.level == crypto::EncryptionLevel::initial) {
                        return 0;
                    }
                    auto index = int(args.level);
                    auto& wsecret = c->crypto.wsecrets[index];
                    auto& rsecret = c->crypto.rsecrets[index];
                    wsecret.secret = ConstByteLen{args.write_secret, args.secret_len};
                    rsecret.secret = ConstByteLen{args.read_secret, args.secret_len};
                    if (!wsecret.secret.valid() || !rsecret.secret.valid()) {
                        return 0;
                    }
                    wsecret.cipher = c->crypto.tls.get_cipher();
                    rsecret.cipher = c->crypto.tls.get_cipher();
                    return 1;
                }
                if (args.type == crypto::ArgType::wsecret) {
                    if (args.level == crypto::EncryptionLevel::initial) {
                        return 0;
                    }
                    auto& wsecret = c->crypto.wsecrets[int(args.level)];
                    wsecret.secret = ConstByteLen{args.write_secret, args.secret_len};
                    if (!wsecret.secret.valid()) {
                        return 0;
                    }
                    wsecret.cipher = args.cipher;
                    return 1;
                }
                if (args.type == crypto::ArgType::rsecret) {
                    if (args.level == crypto::EncryptionLevel::initial) {
                        return 0;
                    }
                    auto& rsecret = c->crypto.rsecrets[int(args.level)];
                    rsecret.secret = ConstByteLen{args.read_secret, args.secret_len};
                    if (!rsecret.secret.valid()) {
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
                    c->crypto.hsdata[int(args.level)].add(args.data, args.len);
                    return 1;
                }
                if (args.type == crypto::ArgType::alert) {
                    c->crypto.tls_alert = args.alert;
                    c->state = QState::tls_alert;
                    return 1;
                }
                if (args.type == crypto::ArgType::flush) {
                    c->crypto.flush_called = true;
                    return 1;
                }
                return 0;
            }

            bool apply_config(QUICContexts* p, Config* c) {
                // ACK handler configs
                if (!c->ack_handler.now || c->ack_handler.msec == 0 ||
                    c->ack_handler.time_threshold.den == 0 ||
                    !c->gen_random) {
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
                p->params.local = c->transport_parameter.params;
                if (!p->params.boxing()) {
                    return false;
                }
                p->srcIDs.gen_rand = std::move(c->gen_random);
                return true;
            }

            dnet_dll_implement(QUIC) make_quic(TLS& tls, Config&& config) {
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
                p->crypto.tls = std::move(tls);
                if (p->crypto.tls.make_quic(qtls_, p).is_error()) {
                    tls = std::move(p->crypto.tls);
                    return {};
                }
                r.cancel();
                return QUIC{p};
            }

            error::Error QUIC::close_reason() {
                if (!p) {
                    return error::none;
                }
                return p->errs.close_err;
            }

            error::Error QUIC::last_error() {
                if (!p) {
                    return error::none;
                }
                return p->errs.err;
            }

            error::Error QUIC::connect() {
                if (!p) {
                    return error::Error("uninitialized", error::ErrorCategory::validationerr);
                }
                while (true) {
                    if (p->state == QState::start) {
                        if (auto err = start_initial_handshake(p)) {
                            set_errors(p, err);
                            return err;
                        }
                        continue;
                    }
                    if (p->state == QState::receving_peer_handshake_packets) {
                        if (auto err = receiving_peer_handshake_packets(p)) {
                            set_errors(p, err);
                            return err;
                        }
                        continue;
                    }
                    if (p->state == QState::sending_local_handshake_packets) {
                        if (auto err = sending_local_handshake_packets(p)) {
                            set_errors(p, err);
                            return err;
                        }
                        continue;
                    }
                    if (p->state == QState::waiting_for_handshake_done) {
                        // handshake not completed yet,
                        // but can send data now
                        return error::none;
                    }
                    if (p->state == QState::on_error) {
                        p->state = QState::fatal;
                        return p->errs.err;
                    }
                    if (p->state == QState::closed) {
                        return error::eof;
                    }
                    break;
                }
                return error::block;
            }
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
*/
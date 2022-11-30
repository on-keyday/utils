/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/quic_contexts.h>
#include <dnet/quic/frame/frame_make.h>
#include <dnet/quic/packet/packet_util.h>
#include <dnet/quic/frame/frame_util.h>
#include <dnet/quic/internal/packet_handler.h>
#include <dnet/quic/crypto/crypto.h>
#include <dnet/quic/internal/gen_packet.h>

namespace utils {
    namespace dnet {
        namespace quic {

            struct CryptoMessageError {
                String dnstr;
                error::Error base;
                void error(auto&& pb) {
                    if (base != error::none) {
                        base.error(pb);
                        helper::append(pb, " ");
                    }
                    helper::append(pb, dnstr);
                }
            };

            error::Error handle_crypto_error(QUICContexts* q, error::Error err) {
                if (isTLSBlock(err)) {
                    return error::none;  // blocked but no error
                }
                String str;
                TLS::get_errors([&](const char* data, size_t len) {
                    str.push_back(' ');
                    str.append(data, len);
                    return 1;
                });
                if (q->state == QState::tls_alert) {
                    return QUICError{
                        .msg = q->crypto.established ? "crypto context error" : "crypto handshake error",
                        .transport_error = to_CRYPTO_ERROR(q->crypto.tls_alert),
                        .frame_type = FrameType::CRYPTO,
                        .base = CryptoMessageError{std::move(str), std::move(err)},
                    };
                }
                return QUICError{
                    .msg = "unknown tls error",
                    .frame_type = FrameType::CRYPTO,
                    .base = CryptoMessageError{std::move(str), std::move(err)},
                };
            }

            error::Error start_tls_handshake(QUICContexts* p) {
                if (!p->params.local.original_dst_connection_id.enough(8)) {
                    p->initialDstID = p->srcIDs.genrandom(20);
                    p->params.local.original_dst_connection_id = {p->initialDstID.data(), 20};
                }
                auto wp = p->get_tmpbuf();
                auto [_, issued] = p->srcIDs.issue(20);
                p->params.local.initial_src_connection_id = issued;
                auto tparam = p->params.local.to_transport_param(wp);
                auto tp = range_aquire(wp, [&](WPacket& w) {
                    for (auto& param : tparam) {
                        if (trsparam::is_defined_both_set_allowed(param.first.id.value)) {
                            if (!param.second) {
                                continue;
                            }
                            auto plen = param.first.render_len();
                            WPacket tmp{w.acquire(plen)};
                            if (!param.first.render(tmp)) {
                                return false;
                            }
                        }
                    }
                    return true;
                });
                if (!tp.valid()) {
                    return QUICError{
                        .msg = "failed to generate transport parameter",
                    };
                }
                if (auto err = p->crypto.tls.set_quic_transport_params(tp.data, tp.len)) {
                    return QUICError{
                        .msg = "failed to set transport parameter",
                        .base = std::move(err),
                    };
                }
                if (auto err = p->crypto.tls.connect()) {
                    return handle_crypto_error(p, std::move(err));
                }
                return error::none;
            }

            error::Error start_initial_handshake(QUICContexts* p) {
                if (auto err = start_tls_handshake(p)) {
                    return err;
                }
                auto crypto_data = p->crypto.hsdata[int(crypto::EncryptionLevel::initial)].get();
                auto cframe = frame::make_crypto(nullptr, 0, crypto_data);
                handler::PacketArg arg;
                arg.dstID = p->params.local.original_dst_connection_id;
                arg.srcID = p->params.local.initial_src_connection_id;
                arg.origDstID = arg.dstID;
                arg.reqlen = 1200;
                arg.version = version_1;
                auto err = handler::generate_initial_packet(p, arg, [&](WPacket& wp, size_t space) {
                    cframe.render(wp);
                    wp.add(0, space - cframe.render_len());
                });
                if (err) {
                    return err;
                }
                p->state = QState::receving_peer_handshake_packets;
                return error::none;
            }

            error::Error receiving_peer_handshake_packets(QUICContexts* q) {
                if (!q->datagrams.size()) {
                    return error::block;
                }
                auto dgram = std::move(q->datagrams.front());
                q->datagrams.pop_front();
                auto unboxed = dgram.b.unbox();
                auto err = handler::recv_QUIC_packets(q, unboxed);
                if (err) {
                    return err;
                }
                return error::none;
            }

            error::Error generate_acks(QUICContexts* q) {
            }

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

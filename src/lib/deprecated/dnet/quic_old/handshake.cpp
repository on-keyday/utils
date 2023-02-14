/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

/*
#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/quic_contexts.h>
// #include <dnet/quic/frame/frame_make.h>
// #include <dnet/quic/packet/packet_util.h>
// #include <dnet/quic/frame/frame_util.h>
#include <dnet/quic/internal/packet_handler.h>
#include <dnet/quic/crypto/crypto.h>
#include <dnet/quic/internal/gen_packet.h>
#include <dnet/quic/internal/gen_frames.h>
#include <dnet/quic/frame/crypto.h>
#include <dnet/quic/version.h>

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

            error::Error start_tls_handshake(QUICContexts* q) {
                if (!q->params.local.original_dst_connection_id.enough(8)) {
                    auto origDst = q->srcIDs.genrandom(20);
                    q->params.local.original_dst_connection_id = {origDst.data(), origDst.size()};
                    if (!q->params.boxing()) {
                        return error::memory_exhausted;
                    }
                }
                auto wp = q->get_tmpbuf();
                auto [_, issued] = q->srcIDs.issue(20);
                q->params.local.initial_src_connection_id = issued;
                auto tparam = q->params.local.to_transport_param(wp);
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
                if (auto err = q->crypto.tls.set_quic_transport_params(tp.data, tp.len)) {
                    return QUICError{
                        .msg = "failed to set transport parameter",
                        .base = std::move(err),
                    };
                }
                if (auto err = q->crypto.tls.connect()) {
                    return handle_crypto_error(q, std::move(err));
                }
                return error::none;
            }

            error::Error start_initial_handshake(QUICContexts* p) {
                // p->set_direction(stream::Direction::client);
                if (auto err = start_tls_handshake(p)) {
                    return err;
                }
                auto crypto_data = p->crypto.hsdata[int(crypto::EncryptionLevel::initial)].get();
                auto cframe = frame::CryptoFrame{.offset = 0, .crypto_data = view::rvec(crypto_data.data, crypto_data.len)};
                handler::PacketArg arg;
                arg.dstID = p->params.local.original_dst_connection_id;
                arg.srcID = p->params.local.initial_src_connection_id;
                arg.origDstID = arg.dstID;
                arg.reqlen = 1200;
                arg.version = version_1;
                arg.payload_len = cframe.len(false);
                /*auto err = handler::generate_initial_packet(p, arg, [&](handler::PacketArg& arg, WPacket& wp, size_t space, size_t pn) {
                    io::writer w{view::wvec(wp.b.data + wp.offset, wp.b.len - wp.offset)};
                    cframe.render(w);
                    w.write(0, space - cframe.len(false));
                    wp.offset += w.offset();
                    arg.ack_eliciting = true;
                    arg.in_flight = true;
                    return error::none;
                });*/
/*if (err) {
    return err;
}*
p->crypto.hsdata[int(crypto::EncryptionLevel::initial)].consume();
p->state = QState::receving_peer_handshake_packets;
return error::none;
}

error::Error receiving_peer_handshake_packets(QUICContexts* q) {
    if (!q->datagrams.size()) {
        return error::block;
    }
    while (q->datagrams.size()) {
        auto dgram = std::move(q->datagrams.front());
        q->datagrams.pop_front();
        auto unboxed = dgram.data.unbox();
        auto err = handler::recv_QUIC_packets(q, unboxed);
        if (err) {
            return err;
        }
    }
    q->state = QState::sending_local_handshake_packets;
    return error::none;
}

error::Error sending_local_handshake_packets(QUICContexts* q) {
    /*if (auto err = handler::generate_frames(q)) {
        return err;
    }
    if (auto err = handler::generate_packets(q)) {
        return err;
    }*
if (q->crypto.established) {
    q->state = QState::waiting_for_handshake_done;
}
return error::none;
}

}  // namespace quic
}  // namespace dnet
}  // namespace utils
*/
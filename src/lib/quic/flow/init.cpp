/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <quic/common/dll_cpp.h>
#include <quic/core/core.h>
#include <quic/internal/context_internal.h>
#include <quic/conn/conn.h>
#include <quic/frame/encode.h>
#include <quic/packet/write_packet.h>
#include <quic/packet/length.h>
#include <quic/frame/length.h>
#include <quic/transport_param/tpparam.h>
#include <quic/crypto/crypto.h>
#include <quic/packet/packet_context.h>
#include <quic/frame/frame_callback.h>
#include <cassert>
#include <quic/flow/init.h>

namespace utils {
    namespace quic {
        namespace flow {

            dll(Error) start_conn_client(core::QUIC* q, InitialConfig config, DataCallback cb) {
                if (!q) {
                    return Error::invalid_argument;
                }
                auto c = conn::new_connection(q, client);
                if (!c) {
                    return Error::none;
                }
                packet::InitialPacket ini{};
                if (config.pnlen < 1 || 4 < config.pnlen) {
                    config.pnlen = 1;
                }
                auto initial = packet::make_fb(packet::types::Initial, config.pnlen, 0);
                byte nul = 0;                      // need to encode
                ini.flags = initial;               // 1
                ini.version = 1;                   // 4
                ini.dstID_len = 0;                 // 1
                ini.dstID = &nul;                  // 0
                ini.srcID_len = config.connIDlen;  // 1
                ini.srcID = config.connID;         // config.connIDlen
                ini.token_len = config.tokenlen;   // varinable length
                ini.token = config.token;          // config.tokenlen
                ini.payload_length = 1200 - 16;    // 2 (temporary)
                ini.packet_number = 0;             // config.pnlen

                auto make_err = [](auto err, auto base) {
                    return Error(int(err) + int(base));
                };
#define RET_IF_ERRF(err, cmp, base)     \
    {                                   \
        if (err != cmp) {               \
            cb(c, nullptr, 0);          \
            return make_err(err, base); \
        }                               \
    }
#define RET_IF_ERR RET_IF_ERRF(cerr, crypto::Error::none, Error::crypto_err)

                auto cerr = crypto::set_alpn(c, config.alpn, config.alpn_len);
                RET_IF_ERR
                cerr = crypto::set_hostname(c, config.host);
                RET_IF_ERR
                cerr = crypto::set_peer_cert(c, config.cert);
                RET_IF_ERR
                cerr = crypto::set_transport_parameter(c, config.transport_param, false, config.custom_param, config.custom_len);
                RET_IF_ERR
                auto cframe = frame::frame<frame::types::CRYPTO>();
                cerr = crypto::start_handshake(c, [&](const byte* data, tsize size, bool err) {
                    if (!err) {
                        bytes::append(cframe.crypto_data, &cframe.length, &q->g->alloc, data, size);
                    }
                });
                tsize header_len = packet::length(&ini);
                tsize payload_len = frame::length(cframe, true);
                tsize padding_len = 1200 - 16 - header_len - payload_len;
                ini.payload_length = payload_len + padding_len;
                bytes::Buffer b{};
                b.a = &q->g->alloc;
                packet::RWContext rp;
                frame::RWContext rf;
                auto perr = packet::write_packet(b, &ini, rp);
                RET_IF_ERRF(perr, packet::Error::none, Error::packet_err)
                assert(b.len == header_len);
                auto ferr = frame::write_frame(b, &cframe, rf);
                RET_IF_ERRF(ferr, frame::Error::none, Error::frame_err)
                assert(b.len == header_len + payload_len);
                ferr = frame::write_padding(b, padding_len, rf);
                RET_IF_ERRF(ferr, frame::Error::none, Error::frame_err)
                assert(b.len == header_len + payload_len + padding_len);
                ini.raw_header = b.b.own();
                ini.raw_payload = b.b.own() + header_len;
                cerr = crypto::encrypt_packet_protection(client, c->q->g->bpool, &ini);
                RET_IF_ERR
                cb(c, b.b.c_str(), b.len);
                b.a->discard_buffer(b.b);
                return Error::none;
#undef RET_IF_ERR
#undef RET_IF_ERRF
            }
        }  // namespace flow
    }      // namespace quic
}  // namespace utils

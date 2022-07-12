/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_next - implement quic::packet::PacketHeadNext
#pragma once

#include "packet_head.h"
#include "../internal/context_internal.h"
#include "../conn/connId.h"

namespace utils {
    namespace quic {
        namespace packet {

            constexpr auto connIDLenLimit = 20;

            struct ReadContext {
                context::Connection* c;
                FirstByte first_byte;
                conn::ConnID dstID;
                conn::ConnID srcID;
                conn::Token token;

                Error discard(auto&& bytes, tsize size, tsize* offset, FirstByte fb, uint version, bool versuc) {
                    return pool::select_bytes_way<Error, Error::none, Error::memory_exhausted>(
                        bytes, c->q->bpool, size, true, 0, [&](const byte* b) {
                            c->callback.on_discard(b, size, *offset, fb, version, versuc);
                            return Error::none;
                        });
                }

                Error save_id(conn::ConnID& id, auto&& bytes, tsize size, tsize* offset, byte len) {
                    if (len > connIDLenLimit) {
                        return Error::too_long_connection_id;
                    }
                    if (size - *offset < len) {
                        return Error::not_enough_length;
                    }
                    // protect from remaining id
                    c->q->bpool.put(std::move(id.id));
                    id.len = conn::InvalidLength;
                    auto dst = c->q->bpool.get(len + 1);
                    byte* target = dst.own();
                    if (!target) {
                        return Error::memory_exhausted;
                    }
                    bytes::copy(target, bytes, *offset + len, *offset);
                    target[len] = 0;  // null terminated
                    id.id = std::move(dst);
                    id.len = len;
                    *offset += len;
                    return Error::none;
                }

                Error save_dst(auto&& bytes, tsize size, tsize* offset, byte len) {
                    return save_id(dstID, bytes, size, offset, len);
                }

                Error save_dst_known(auto&& bytes, tsize size, tsize* offset) {
                    c->q->bpool.put(std::move(dstID.id));
                    dstID.len = conn::InvalidLength;
                    conn::ConnID known = c->srcIDs.get_known(c->q->bpool, bytes, size, offset, connIDLenLimit);
                    if (known.len == conn::InvalidLength) {
                        return Error::unkown_connection_id;
                    }
                    dstID = std::move(known);
                    *offset += dstID.len;
                    return Error::none;
                }

                Error save_src(auto&& bytes, tsize size, tsize* offset, byte len) {
                    return save_id(srcID, bytes, size, offset, len);
                }

                Error save_initial_token(auto&& bytes, tsize size, tsize* offset, byte len) {
                    if (len > connIDLenLimit) {
                        return Error::too_long_connection_id;
                    }
                    if (size - *offset < len) {
                        return Error::not_enough_length;
                    }
                    c->q->bpool.put(std::move(token.token));
                    token.len = conn::InvalidLength;
                    bytes::Bytes tok = c->q->bpool.get(len + 1);
                    byte* target = tok.own();
                    if (!target) {
                        return Error::memory_exhausted;
                    }
                    bytes::copy(target, bytes, *offset + len, *offset);
                    *offset += len;
                    target[len] = 0;  // null terminated
                    token.token = std::move(tok);
                    token.len = len;
                    return Error::none;
                }

                Error long_h(auto&& bytes, tsize size, tsize* offset, FirstByte fb, tsize payload_len) {
                    first_byte = fb;
                }

                void packet_error(auto&& bytes, tsize size, tsize* offset, FirstByte fb, uint version, Error err, const char* where) {
                    pool::select_bytes_way(
                        bytes, c->q->g->bpool, size, c->q->g->callback.on_packet_error_f || c->callback.on_packet_error_f, *offset,
                        [&](const byte* b) {
                            c->callback.on_packet_error(b, size, *offset, fb, version, err, where);
                            c->q->g->callback.on_packet_error(b, size, *offset, fb, version, err, where);
                        });
                }

                ~ReadContext() {
                    c->q->g->bpool.put(std::move(dstID.id));
                    c->q->g->bpool.put(std::move(srcID.id));
                    c->q->g->bpool.put(std::move(token.token));
                }
            };
        }  // namespace packet
    }      // namespace quic
}  // namespace utils

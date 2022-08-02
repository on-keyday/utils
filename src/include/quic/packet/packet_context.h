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
#include "../mem/vec.h"
#include "../mem/callback.h"

namespace utils {
    namespace quic {
        namespace packet {

            struct ReadContext;
            template <class Packet, class Ctx>
            using PCB = mem::CB<Ctx*, Error, Packet*>;

            template <class Ctx>
            struct ReadCallback {
                PCB<InitialPacket, Ctx> initial;
                PCB<RetryPacket, Ctx> retry;
                PCB<ZeroRTTPacket, Ctx> zero_rtt;
                PCB<HandshakePacket, Ctx> handshake;
                PCB<VersionNegotiationPacket, Ctx> version;
                PCB<OneRTTPacket, Ctx> one_rtt;
                mem::CB<Ctx*, void, Error, const char*, Packet*> packet_error;
                mem::CB<Ctx*, void, varint::Error, const char*, Packet*> varint_error;
            };

            struct RWContext {
                // TODO(on-keyday): replace q to c
                // conn::Connection* c;
                core::QUIC* q;
                ReadCallback<RWContext> cb;

                Error discard(byte* bytes, tsize size, tsize* offset, FirstByte fb, uint version, bool versuc) {
                    return Error::none;
                }

                void packet_error(Error err, const char* where, Packet* p = nullptr) {
                    cb.packet_error(this, err, where, p);
                }
                void varint_error(varint::Error err, const char* where, Packet* p = nullptr) {
                    cb.varint_error(this, err, where, p);
                }

                bytes::Bytes get_bytes(tsize s) {
                    return q->g->bpool.get(s);
                }

                void put_bytes(bytes::Bytes&& b) {
                    q->g->bpool.put(std::move(b));
                }

                bool acceptable_version(uint version) {
                    return version == 1;
                }

                mem::Vec<conn::ConnID>* get_issued_connid(byte first) {
                    return nullptr;
                }

                Error initial(InitialPacket* p) {
                    return cb.initial(this, p);
                }
                Error zero_rtt(ZeroRTTPacket* p) {
                    return cb.zero_rtt(this, p);
                }
                Error handshake(HandshakePacket* p) {
                    return cb.handshake(this, p);
                }
                Error retry(RetryPacket* p) {
                    return cb.retry(this, p);
                }
                Error version(VersionNegotiationPacket* p) {
                    return cb.version(this, p);
                }
                Error one_rtt(OneRTTPacket* p) {
                    return cb.one_rtt(this, p);
                }
            };
        }  // namespace packet
    }      // namespace quic
}  // namespace utils

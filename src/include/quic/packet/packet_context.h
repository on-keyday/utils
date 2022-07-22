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

namespace utils {
    namespace quic {
        namespace packet {
            template <class T, class Req>
            concept void_or_type = std::is_same_v<Req, void> || std::is_same_v<T, Req>;

            template <class T, class Ret, class... Args>
            concept callable = requires(T t) {
                { t(std::declval<Args>()...) } -> void_or_type<Ret>;
            };

            template <class Ret>
            auto select_call(Ret default_v, auto&& callback) {
                if constexpr (callable<decltype(callback), Ret>) {
                    return callback();
                }
                else {
                    callback();
                    return default_v;
                }
            }

            template <class Packet, class Ctx = void>
            struct PCB {
                void* func_ctx = nullptr;
                using callback_t = Error (*)(void*, Packet*, Ctx*);
                callback_t cb = nullptr;
                Error operator()(Packet* p, Ctx* ctx) {
                    return cb ? cb(func_ctx, p, ctx) : Error::none;
                }
                constexpr PCB() = default;
                constexpr PCB(std::nullptr_t)
                    : PCB() {}
                constexpr PCB(callback_t cb, void* ctx)
                    : cb(cb), func_ctx(ctx) {}
                constexpr PCB(auto&& v) {
                    auto ptr = std::addressof(v);
                    if constexpr (std::is_same_v<decltype(ptr), callback_t>) {
                        cb = ptr;
                        func_ctx = nullptr;
                    }
                    else {
                        cb = [](void* c, Packet* p, Ctx* ctx) -> Error {
                            using Ptr = decltype(ptr);
                            auto& call = (*static_cast<Ptr>(c));
                            using T = decltype(call);
                            if constexpr (callable<T, void, Packet*, Ctx*>) {
                                return select_call(Error::none, [&] {
                                    return call(p, ctx);
                                });
                            }
                            else if constexpr (callable<T, void, Packet*>) {
                                return select_call(Error::none, [&] {
                                    return call(p);
                                });
                            }
                            else {
                                return select_call(Error::none, [&] {
                                    return call();
                                });
                            }
                        };
                        func_ctx = ptr;
                    }
                }
            };

            template <class Ctx>
            struct ReadCallback {
                PCB<InitialPacket, Ctx> initial;
                PCB<RetryPacket, Ctx> retry;
                PCB<ZeroRTTPacket, Ctx> zero_rtt;
                PCB<HandshakePacket, Ctx> handshake;
                PCB<VersionNegotiationPacket, Ctx> version;
                PCB<OneRTTPacket, Ctx> one_rtt;
            };

            struct ReadContext {
                // TODO(on-keyday): replace q to c
                // conn::Connection* c;
                core::QUIC* q;
                ReadCallback<ReadContext> cb;

                Error discard(byte* bytes, tsize size, tsize* offset, FirstByte fb, uint version, bool versuc) {
                    return Error::none;
                }

                void packet_error(Error err, const char* where, Packet* p) {}
                void varint_error(varint::Error err, const char* where, Packet* p) {}

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
                    return cb.initial(p, this);
                }
                Error zero_rtt(ZeroRTTPacket* p) {
                    return cb.zero_rtt(p, this);
                }
                Error handshake(HandshakePacket* p) {
                    return cb.handshake(p, this);
                }
                Error retry(RetryPacket* p) {
                    return cb.retry(p, this);
                }
                Error version(VersionNegotiationPacket* p) {
                    return cb.version(p, this);
                }
                Error one_rtt(OneRTTPacket* p) {
                    return cb.one_rtt(p, this);
                }
            };
        }  // namespace packet
    }      // namespace quic
}  // namespace utils

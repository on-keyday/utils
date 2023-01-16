/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// encode - transport parameter encoding
#pragma once
#include "../common/variable_int.h"
#include "../mem/bytes.h"
#include "../mem/alloc.h"
#include "../mem/pool.h"
#include <cassert>

namespace utils {
    namespace quic {
        namespace tpparam {
            struct TransportParam {
                tsize id;
                tsize len;
                const byte* data;
            };

            inline varint::Error decode(TransportParam& param, const byte* bytes, tsize size, tsize* offset) {
                tsize id, len, red;
                auto err = varint::decode(bytes, &id, &red, size, *offset);
                if (err != varint::Error::none) {
                    return err;
                }
                param.id = id;
                *offset += red;
                err = varint::decode(bytes, &len, &red, size, *offset);
                if (err != varint::Error::none) {
                    return err;
                }
                param.len = len;
                *offset += red;
                if (*offset + len >= size) {
                    return varint::Error::need_more_length;
                }
                param.data = bytes;
                *offset += len;
                return varint::Error::none;
            }

            inline varint::Error encode(bytes::Buffer& b, const TransportParam& param, bool nparam = false) {
                auto idlen = varint::least_enclen(param.id);
                auto lenlen = varint::least_enclen(param.len);
                if (!idlen || !lenlen) {
                    return varint::Error::too_large_number;
                }
                byte s[16];
                tsize offset = b.len;
                auto ok = append(b, s, idlen + lenlen);
                if (ok <= 0) {
                    return varint::Error::need_more_length;
                }
                auto err = varint::encode(b.b.own(), param.id, idlen, b.len, &offset);
                if (err != varint::Error::none) {
                    return err;
                }
                err = varint::encode(b.b.own(), param.id, lenlen, b.len, &offset);
                if (err != varint::Error::none) {
                    return err;
                }
                assert(b.len == offset);
                if (nparam) {
                    return varint::Error::none;
                }
                return append(b, param.data, param.len) <= 0
                           ? varint::Error::need_more_length
                           : varint::Error::none;
            }

            struct PreferredAddress {
                struct {
                    union {
                        uint v;
                        byte raw[4];
                    } address;
                    union {
                        ushort v;
                        byte raw[2];
                    } port;
                } ipv4;
                struct {
                    union {
                        struct {
                            tsize high;
                            tsize low;
                        };
                        byte raw[16];
                    } address;
                    union {
                        ushort v;
                        byte raw[2];
                    } port;
                } ipv6;
                byte connIDlen;
                const byte* connID;
                byte stateless_reset_token[16];
            };

            // 18.2.  Transport Parameter Definitions
            struct DefinedParams {
                struct {
                    const byte* dstID;
                    tsize len;
                } original_dst_connection_id{};  // 0x00
                tsize max_idle_timeout = 0;      // 0x01
                struct {
                    bool has;
                    byte token[16];  // 0x02
                } stateless_reset{};
                tsize max_udp_payload_size = 65527;
                tsize initial_max_data = 0;
                tsize initial_max_stream_data_bidi_local = 0;
                tsize initial_max_stream_data_bidi_remote = 0;
                tsize initial_max_stream_data_uni = 0;
                tsize initial_max_streams_bidi = 0;
                tsize initial_max_streams_uni = 0;
                tsize ack_delay_exponent = 3;
                tsize max_ack_delay = 25;
                bool disable_active_migration = false;
                struct {
                    bool has;
                    PreferredAddress addr;
                } preferred_address{};
                tsize active_connection_id_limit = 2;
                struct {
                    const byte* srcID;
                    tsize len;
                } initial_src_connection_id{};
                struct {
                    const byte* srcID;
                    tsize len;
                } retry_src_connection_id{};
                tsize max_datagram_frame_size = 0;  // rfc9221 3. Transport Parameter
            };

            constexpr DefinedParams defaults = {};

            inline varint::Error encode(bytes::Buffer& b, const DefinedParams& params, bool force_all) {
                TransportParam param{};
                auto encode_param = [&](tsize id, const byte* data, tsize len, bool nparam) {
                    param.id = id;
                    param.len = len;
                    param.data = data;
                    auto err = encode(b, param);
                    if (err != varint::Error::none) {
                        return err;
                    }
                    return varint::Error::none;
                };
                auto encode_int = [&](tsize id, tsize data) {
                    byte tmpbuf[4]{};
                    tsize red = 0;
                    auto err = varint::encode(tmpbuf, params.max_idle_timeout, 0, 4, &red);
                    if (err != varint::Error::none) {
                        return err;
                    }
                    return encode_param(id, tmpbuf, red, false);
                };
#define ER(id, data, len, nparam)                                                         \
    {                                                                                     \
        if (auto err = encode_param(id, data, len, nparam); err != varint::Error::none) { \
            return err;                                                                   \
        }                                                                                 \
    }
#define E(id, data, len) ER(id, data, len, false)
#define R(data, len)                                \
    {                                               \
        if (append(b, data, len) <= 0) {            \
            return varint::Error::need_more_length; \
        }                                           \
    }
#define Z(id) E(id, nullptr, 0)
#define I(id, data)                                                        \
    {                                                                      \
        if (auto err = encode_int(id, data); err != varint::Error::none) { \
            return err;                                                    \
        }                                                                  \
    }
#define ALL force_all ||
                if (params.original_dst_connection_id.dstID) {
                    E(0x00, params.original_dst_connection_id.dstID, params.original_dst_connection_id.len)
                }
                if (ALL params.max_idle_timeout != 0) {
                    I(0x01, params.max_idle_timeout)
                }
                if (params.stateless_reset.has) {
                    E(0x02, params.stateless_reset.token, 16)
                }
                if (ALL params.max_udp_payload_size != 65527) {
                    I(0x03, params.max_udp_payload_size)
                }
                if (ALL params.initial_max_data) {
                    I(0x04, params.initial_max_data)
                }
                if (ALL params.initial_max_stream_data_bidi_local) {
                    I(0x05, params.initial_max_stream_data_bidi_local)
                }
                if (ALL params.initial_max_stream_data_bidi_remote) {
                    I(0x06, params.initial_max_stream_data_bidi_remote)
                }
                if (ALL params.initial_max_stream_data_uni) {
                    I(0x07, params.initial_max_stream_data_uni)
                }
                if (ALL params.initial_max_streams_bidi) {
                    I(0x08, params.initial_max_streams_bidi)
                }
                if (ALL params.initial_max_streams_uni) {
                    I(0x09, params.initial_max_streams_uni)
                }
                if (ALL params.ack_delay_exponent != 3) {
                    I(0x0a, params.ack_delay_exponent)
                }
                if (ALL params.max_ack_delay != 25) {
                    I(0x0b, params.max_ack_delay)
                }
                if (ALL params.disable_active_migration) {
                    Z(0x0c)
                }
                if (params.preferred_address.has) {
                    auto& a = params.preferred_address.addr;
                    ER(0x0d, nullptr, 24 /*addresss*/ + 1 /*len*/ + a.connIDlen + 16 /*stateless_reset*/, true)
                    R(a.ipv4.address.raw, 4)
                    R(a.ipv4.port.raw, 2)
                    R(a.ipv6.address.raw, 16)
                    R(a.ipv6.port.raw, 2)
                    R(&a.connIDlen, 1)
                    R(a.connID, a.connIDlen)
                    R(a.stateless_reset_token, 16)
                }
                if (ALL params.active_connection_id_limit) {
                    I(0x0e, params.active_connection_id_limit)
                }
                if (params.initial_src_connection_id.srcID) {
                    E(0x0f, params.initial_src_connection_id.srcID, params.initial_src_connection_id.len)
                }
                if (params.retry_src_connection_id.srcID) {
                    E(0x10, params.retry_src_connection_id.srcID, params.retry_src_connection_id.len)
                }
                if (ALL params.max_datagram_frame_size) {
                    I(0x20, params.max_datagram_frame_size)
                }
#undef E
#undef ER
#undef R
#undef Z
#undef I
                return varint::Error::none;
            }

            inline varint::Error set_defined_params(DefinedParams& params, const TransportParam& param) {
#define I(to)                                                            \
    {                                                                    \
        tsize redsize = 0;                                               \
        auto err = varint::decode(param.data, &to, &redsize, param.len); \
        if (err != varint::Error::none) {                                \
            return err;                                                  \
        }                                                                \
    }
#define RET return varint::Error::none;
#define IRET(to) I(to) RET
                switch (param.id) {
                    case 0x00: {
                        params.original_dst_connection_id.dstID = param.data;
                        params.original_dst_connection_id.len = param.len;
                        RET
                    }
                    case 0x01: {
                        IRET(params.max_idle_timeout)
                    }
                    case 0x02: {
                        if (param.len != 16) {
                            return varint::Error::need_more_length;
                        }
                        bytes::copy(params.stateless_reset.token, param.data, param.len);
                        params.stateless_reset.has = true;
                        RET
                    }
                    case 0x03: {
                        IRET(params.max_udp_payload_size)
                    }
                    case 0x04: {
                        IRET(params.initial_max_data)
                    }
                    case 0x05: {
                        IRET(params.initial_max_stream_data_bidi_local)
                    }
                    case 0x06: {
                        IRET(params.initial_max_stream_data_bidi_remote)
                    }
                    case 0x07: {
                        IRET(params.initial_max_stream_data_uni)
                    }
                    case 0x08: {
                        IRET(params.initial_max_streams_bidi)
                    }
                    case 0x09: {
                        IRET(params.initial_max_streams_uni)
                    }
                    case 0x0a: {
                        IRET(params.ack_delay_exponent)
                    }
                    case 0x0b: {
                        IRET(params.max_ack_delay)
                    }
                    case 0x0c: {
                        params.disable_active_migration = true;
                        RET
                    }
                    case 0x0d: {
                        tsize offset = 0;
                        if (param.len <= 24 + 1) {
                            return varint::Error::need_more_length;
                        }
                        auto& a = params.preferred_address.addr;
                        bytes::copy(a.ipv4.address.raw, param.data, 4);
                        offset += 4;
                        bytes::copy(a.ipv4.port.raw, param.data + offset, 2);
                        offset += 2;
                        bytes::copy(a.ipv6.address.raw, param.data + offset, 16);
                        offset += 16;
                        bytes::copy(a.ipv6.port.raw, param.data + offset, 2);
                        offset += 2;
                        byte len = param.data[offset];
                        a.connIDlen = len;
                        offset += 1;
                        if (param.len - offset < len + 16) {
                            return varint::Error::need_more_length;
                        }
                        a.connID = param.data + offset;
                        offset += len;
                        bytes::copy(a.stateless_reset_token, param.data + offset, 16);
                        params.preferred_address.has = true;
                        RET
                    }
                    case 0x0e: {
                        IRET(params.active_connection_id_limit)
                    }
                    case 0x0f: {
                        params.initial_src_connection_id.srcID = param.data;
                        params.initial_src_connection_id.len = param.len;
                        RET
                    }
                    case 0x10: {
                        params.retry_src_connection_id.srcID = param.data;
                        params.retry_src_connection_id.len = param.len;
                        RET
                    }
                    case 0x20: {
                        IRET(params.max_datagram_frame_size)
                    }
                    default:
                        return varint::Error::none;
                }
#undef I
#undef RET
#undef IRET
            }
        }  // namespace tpparam
    }      // namespace quic
}  // namespace utils

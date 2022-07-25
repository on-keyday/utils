/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// write_packet - write packet
#pragma once
#include "packet_head.h"
#include "../mem/buf_write.h"

namespace utils {
    namespace quic {
        namespace packet {
#define WRITE(data, len, where)                          \
    {                                                    \
        auto err = mem::append_buf<Error>(b, data, len); \
        if (err != Error::none) {                        \
            next.packet_error(err, where);               \
            return err;                                  \
        }                                                \
    }
#define ENCODE(data, where)                                  \
    {                                                        \
        varint::Error verr;                                  \
        auto err = mem::write_varint<Error>(b, data, &verr); \
        if (err != Error::none) {                            \
            next.varint_error(verr, where);                  \
            return err;                                      \
        }                                                    \
    }

            template <class Next>
            Error write_packet_number_and_length(bytes::Buffer& b, Packet* p, Next&& next, const char* where_pn, const char* where_length) {
                auto pn = p->flags.packet_number_length();
                varint::Swap<uint> swp;
                swp.t = p->packet_number;
                varint::reverse_if(swp);
                if (where_length) {
                    auto length = p->payload_length + pn;
                    ENCODE(length, where)
                }
                WRITE(swp.swp + 4 - pn, pn, where);
                return Error::none;
            }

            template <class Next>
            Error write_initial_packet(bytes::Buffer& b, InitialPacket* p, Next&& next) {
                ENCODE(p->token_len, "write_initial_packet/token_len")
                WRITE(p->token, p->token_len, "write_initial_packet/token")
                return write_packet_number(b, p, next,
                                           "write_initial_packet/packet_number",
                                           "write_initial_packet/length");
            }

            template <class Next>
            Error write_handshake_packet(bytes::Buffer& b, HandshakePacket* p, Next&& next) {
                return write_packet_number(b, p, next,
                                           "write_handsahke_packet/packet_number",
                                           "write_handshake_packet/length");
            }

            template <class Next>
            Error write_0rtt_packet(bytes::Buffer& b, ZeroRTTPacket* p, Next&& next) {
                return write_packet_number(b, p, next,
                                           "write_0rtt_packet/packet_number",
                                           "write_0rtt_packet/length");
            }

            template <class Next>
            Error write_retry_packet(bytes::Buffer& b, RetryPacket* p, Next&& next) {
                WRITE(p->retry_token, p->retry_token_len, "write_retry_token/retry_token")
                WRITE(p->integrity_tag, 16, "write_retry_token/integrity_tag")
                return Error::none;
            }

            template <class Next>
            Error write_version_negotiation_packet(bytes::Buffer& b, VersionNegotiationPacket* p, Next&& next) {
                // TODO(on-keyday): write more message
                return Error::none;
            }

            template <class Next>
            Error write_long_packet(bytes::Buffer& b, LongPacket* p, Next&& next) {
                varint::Swap<uint> swp{p->version};
                varint::reverse_if(swp);
                WRITE(swp.swp, 4, "write_long_packet/version")
                WRITE(&p->dstID_len, 1, "write_long_packet/dstIDlen")
                WRITE(p->dstID, len, "write_long_packet/dstID")
                WRITE(&p->srcID_len, 1, "write_long_packet/srcIDlen")
                WRITE(p->srcID, len, "write_long_packet/srcID")
                switch (p->flags.type()) {
                    case types::Initial: {
                        return write_initial_packet(b, static_cast<InitialPacket*>(p), next);
                    }
                    case types::HandShake: {
                        return write_handshake_packet(b, static_cast<HandshakePacket*>(p), next);
                    }
                    case types::ZeroRTT: {
                        return write_0rtt_packet(b, static_cast<ZeroRTTPacket*>(p), next);
                    }
                    case types::Retry: {
                        return write_retry_packet(b, static_cast<RetryPacket*>(p), next);
                    }
                    case types::VersionNegotiation: {
                        return write_version_negotiation_packet(b, static_cast<VersionNegotiationPacket*>(p), next);
                    }
                    default:
                        return Error::invalid_fbinfo;
                }
            }

            template <class Next>
            Error write_short_packet(bytes::Buffer& b, packet::OneRTTPacket* p, Next&& next) {
                WRITE(p->dstID, p->dstID_len, "write_short_packet/dstID")
                return write_packet_number_and_length(b, p, next, "write_short_header/packet_number", nullptr);
            }

            // write_packet write packet
            // this method does't write packet payload
            template <class Next>
            Error write_packet(bytes::Buffer& b, packet::Packet* p, Next&& next) {
                if (!p) {
                    return Error::invalid_argument;
                }
                WRITE(&p->flags.raw, 1, "write_packet/type");
                if (p->flags.is_short()) {
                    return write_short_packet(b, static_cast<packet::OneRTTPacket*>(p), next);
                }
                return write_long_packet(b, p, next);
            }
        }  // namespace packet
    }      // namespace quic
}  // namespace utils

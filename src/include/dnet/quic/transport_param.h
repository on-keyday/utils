/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// transport_param - transport parameter
#pragma once
#include "crypto.h"
#include <array>

namespace utils {
    namespace dnet {
        namespace quic {
            struct TransportParameter {
                ByteLen id;
                ByteLen len;
                ByteLen data;

                constexpr bool parse(ByteLen& b) {
                    if (!b.varintfwd(id)) {
                        return false;
                    }
                    if (!b.varintfwd(len)) {
                        return false;
                    }
                    auto l = len.varint();
                    if (!b.enough(l)) {
                        return false;
                    }
                    data = b.resized(l);
                    b = b.forward(l);
                    return true;
                }

                constexpr size_t param_len() const {
                    return id.len + len.len + data.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!id.is_varint_valid() || !len.is_varint_valid()) {
                        return false;
                    }
                    auto l = len.varint();
                    if (!data.enough(l)) {
                        return false;
                    }
                    w.append(id.data, id.varintlen());
                    w.append(len.data, len.varintlen());
                    w.append(data.data, l);
                    return true;
                }

                bool valid() const {
                    if (!id.is_varint_valid() || !len.is_varint_valid() || !data.valid()) {
                        return false;
                    }
                    return data.len >= len.varint();
                }
            };

            enum class DefinedTransportParamID {
                original_dst_connection_id = 0x00,
                max_idle_timeout = 0x01,
                stateless_reset_token = 0x02,
                max_udp_payload_size = 0x03,
                initial_max_data = 0x04,
                initial_max_stream_data_bidi_local = 0x05,
                initial_max_stream_data_bidi_remote = 0x06,
                initial_max_stream_data_uni = 0x07,
                initial_max_streams_bidi = 0x08,
                initial_max_streams_uni = 0x09,
                ack_delay_exponent = 0x0a,
                max_ack_delay = 0x0b,
                disable_active_migration = 0x0c,
                preferred_address = 0x0d,
                active_connection_id_limit = 0x0e,
                initial_src_connection_id = 0x0f,
                retry_src_connection_id = 0x10,
                max_datagram_frame_size = 0x20,
            };

            struct PreferredAddress {
                ByteLen ipv4_address;
                ByteLen ipv4_port;
                ByteLen ipv6_address;
                ByteLen ipv6_port;
                byte* connectionID_length;
                ByteLen connectionID;
                ByteLen stateless_reset_token;

                constexpr bool parse(ByteLen& b) {
                    if (!b.fwdresize(ipv4_address, 0, 4)) {
                        return false;
                    }
                    if (!b.fwdresize(ipv4_port, 4, 2)) {
                        return false;
                    }
                    if (!b.fwdresize(ipv6_address, 2, 16)) {
                        return false;
                    }
                    if (!b.fwdresize(ipv6_port, 16, 2)) {
                        return false;
                    }
                    b = b.forward(2);
                    if (!b.enough(1)) {
                        return false;
                    }
                    connectionID_length = b.data;
                    if (!b.fwdresize(connectionID, 1, *connectionID_length)) {
                        return false;
                    }
                    if (!b.fwdresize(stateless_reset_token, *connectionID_length, 16)) {
                        return false;
                    }
                    return true;
                }

                constexpr size_t param_len() const {
                    return ipv4_address.len + ipv4_port.len + ipv6_address.len + ipv6_port.len +
                           1 +  // length
                           connectionID.len + stateless_reset_token.len;
                }

                constexpr bool render(WPacket& w) const {
                    if (!ipv4_address.enough(4) ||
                        !ipv4_port.enough(2) ||
                        !ipv6_address.enough(16) ||
                        !ipv6_port.enough(2) ||
                        !connectionID_length ||
                        !connectionID.enough(*connectionID_length) ||
                        !stateless_reset_token.enough(16)) {
                        return false;
                    }
                    w.append(ipv4_address.data, 4);
                    w.append(ipv4_port.data, 2);
                    w.append(ipv6_address.data, 16);
                    w.append(ipv6_port.data, 2);
                    w.append(connectionID_length, 1);
                    w.append(connectionID.data, *connectionID_length);
                    w.append(stateless_reset_token.data, 16);
                    return true;
                }
            };

            constexpr TransportParameter make_transport_param(DefinedTransportParamID id, ByteLen data, WPacket& src) {
                auto idv = src.varint(size_t(id));
                if (!idv.is_varint_valid()) {
                    return {};
                }
                auto lenv = src.varint(data.len);
                if (!lenv.is_varint_valid()) {
                    return {};
                }
                return {idv, lenv, data};
            }

            constexpr TransportParameter make_transport_param(DefinedTransportParamID id, size_t data, WPacket& src) {
                auto datav = src.varint(data);
                if (!datav.is_varint_valid()) {
                    return {};
                }
                return make_transport_param(id, datav, src);
            }

            struct DefinedTransportParams {
                ByteLen original_dst_connection_id;
                size_t max_idle_timeout = 0;
                ByteLen stateless_reset_token;
                size_t max_udp_payload_size = 65527;
                size_t initial_max_data = 0;
                size_t initial_max_stream_data_bidi_local = 0;
                size_t initial_max_stream_data_bidi_remote = 0;
                size_t initial_max_stream_data_uni = 0;
                size_t initial_max_streams_bidi = 0;
                size_t initial_max_streams_uni = 0x09;
                size_t ack_delay_exponent = 3;
                size_t max_ack_delay = 25;
                bool disable_active_migration = false;
                PreferredAddress preferred_address;
                size_t active_connection_id_limit = 2;
                ByteLen initial_src_connection_id;
                ByteLen retry_src_connection_id;
                size_t max_datagram_frame_size = 0;

                bool from_transport_param(TransportParameter param) {
                    if (!param.id.is_varint_valid()) {
                        return 0;
                    }
                    auto id = param.id.varint();
#define VLINT(ID)                                                     \
    if (DefinedTransportParamID(id) == DefinedTransportParamID::ID) { \
        if (!param.data.is_varint_valid()) {                          \
            return false;                                             \
        }                                                             \
        ID = param.data.varint();                                     \
        return true;                                                  \
    }
#define RDATA(ID)                                                     \
    if (DefinedTransportParamID(id) == DefinedTransportParamID::ID) { \
        if (!param.data.valid()) {                                    \
            return false;                                             \
        }                                                             \
        ID = param.data;                                              \
    }
                    RDATA(original_dst_connection_id);
                    VLINT(max_idle_timeout);
                    RDATA(stateless_reset_token);
                    VLINT(max_udp_payload_size);
                    VLINT(initial_max_data);
                    VLINT(initial_max_stream_data_bidi_local);
                    VLINT(initial_max_stream_data_bidi_remote);
                    VLINT(initial_max_stream_data_uni);
                    VLINT(initial_max_streams_bidi);
                    VLINT(initial_max_streams_uni);
                    VLINT(ack_delay_exponent);
                    VLINT(max_ack_delay);
                    if (DefinedTransportParamID(id) == DefinedTransportParamID::disable_active_migration) {
                        disable_active_migration = true;
                        return true;
                    }
                    if (DefinedTransportParamID(id) == DefinedTransportParamID::preferred_address) {
                        auto copy = param.data;
                        return preferred_address.parse(copy);
                    }
                    VLINT(active_connection_id_limit);
                    RDATA(initial_src_connection_id);
                    RDATA(retry_src_connection_id);
                    VLINT(max_datagram_frame_size);
                    return true;  // not matched but no error
#undef RDATA
#undef VLINT
                }

                std::array<TransportParameter, 18> to_transport_param(WPacket& src) {
                    std::array<TransportParameter, 18> params;
#define MAKE(i, param)                                                               \
    {                                                                                \
        auto tmp = make_transport_param(DefinedTransportParamID::param, param, src); \
        params[i] = tmp;                                                             \
    }
                    MAKE(0, original_dst_connection_id);
                    MAKE(1, max_idle_timeout);
                    MAKE(2, stateless_reset_token);
                    MAKE(3, max_udp_payload_size);
                    MAKE(4, initial_max_data);
                    MAKE(5, initial_max_stream_data_bidi_local);
                    MAKE(6, initial_max_stream_data_bidi_remote);
                    MAKE(7, initial_max_stream_data_uni);
                    MAKE(8, initial_max_streams_bidi);
                    MAKE(9, initial_max_streams_uni);
                    MAKE(10, ack_delay_exponent);
                    MAKE(11, max_ack_delay);
                    if (disable_active_migration) {
                        params[12] = make_transport_param(DefinedTransportParamID::disable_active_migration, src.acquire(0), src);
                    }
                    else {
                        params[12] = make_transport_param(DefinedTransportParamID::disable_active_migration, ByteLen{}, src);
                    }
                    WPacket tmp;
                    tmp.b = src.acquire(preferred_address.param_len());
                    if (preferred_address.render(tmp)) {
                        params[13] = make_transport_param(DefinedTransportParamID::preferred_address, tmp.b.resized(tmp.offset), src);
                    }
                    else {
                        params[13] = make_transport_param(DefinedTransportParamID::preferred_address, ByteLen{}, src);
                    }
                    MAKE(14, active_connection_id_limit);
                    MAKE(15, initial_src_connection_id);
                    MAKE(16, retry_src_connection_id);
                    MAKE(17, max_datagram_frame_size);
#undef MAKE
                    return params;
                }
            };

            constexpr bool is_defined_both_set_allowed(int id_raw) {
                const auto id = DefinedTransportParamID(id_raw);
                return id == DefinedTransportParamID::max_idle_timeout ||
                       id == DefinedTransportParamID::max_udp_payload_size ||
                       id == DefinedTransportParamID::initial_max_data ||
                       id == DefinedTransportParamID::initial_max_stream_data_bidi_local ||
                       id == DefinedTransportParamID::initial_max_stream_data_bidi_remote ||
                       id == DefinedTransportParamID::initial_max_stream_data_uni ||
                       id == DefinedTransportParamID::initial_max_streams_bidi ||
                       id == DefinedTransportParamID::initial_max_streams_uni ||
                       id == DefinedTransportParamID::ack_delay_exponent ||
                       id == DefinedTransportParamID::max_ack_delay ||
                       id == DefinedTransportParamID::disable_active_migration ||
                       id == DefinedTransportParamID::active_connection_id_limit ||
                       id == DefinedTransportParamID::initial_src_connection_id ||
                       id == DefinedTransportParamID::max_datagram_frame_size;
            }

            constexpr bool is_defined_server_set_allowed(int id_raw) {
                const auto id = DefinedTransportParamID(id_raw);
                return is_defined_both_set_allowed(id_raw) ||
                       id == DefinedTransportParamID::original_dst_connection_id ||
                       id == DefinedTransportParamID::preferred_address ||
                       id == DefinedTransportParamID::stateless_reset_token ||
                       id == DefinedTransportParamID::retry_src_connection_id;
            }
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils

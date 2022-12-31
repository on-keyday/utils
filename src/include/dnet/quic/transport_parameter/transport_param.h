/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// transport_param - transport parameter
#pragma once
#include "../../bytelen.h"
#include "../../boxbytelen.h"
#include <cstdint>
#include "../../easy/array.h"
#include "../../../view/iovec.h"

namespace utils {
    namespace dnet {
        namespace quic {
            namespace trsparam {
                struct TransportParamValue {
                   private:
                    bool is_varint = false;
                    union {
                        ByteLen byts;
                        QVarInt qvint;
                    };

                   public:
                    constexpr TransportParamValue(const TransportParamValue&) = default;
                    constexpr TransportParamValue& operator=(const TransportParamValue&) = default;

                    constexpr TransportParamValue()
                        : byts(ByteLen{}), is_varint(false) {}

                    constexpr TransportParamValue(ByteLen v)
                        : byts(v), is_varint(false) {}

                    constexpr TransportParamValue(size_t v)
                        : qvint(v), is_varint(true) {}

                    constexpr bool parse(ByteLen& b, size_t len) {
                        if (!b.enough(len)) {
                            return false;
                        }
                        byts = b.resized(len);
                        b = b.forward(len);
                        is_varint = false;
                        return true;
                    }

                    constexpr size_t parse_len() const {
                        if (is_varint) {
                            return qvint.len;
                        }
                        else {
                            return byts.len;
                        }
                    }

                    constexpr bool render(WPacket& w) const {
                        if (is_varint) {
                            return qvint.render(w);
                        }
                        else {
                            if (!byts.data && byts.len) {
                                return false;
                            }
                            w.append(byts.data, byts.len);
                            return true;
                        }
                    }

                    constexpr size_t render_len() const {
                        if (is_varint) {
                            return qvint.minimum_len();
                        }
                        else {
                            return byts.len;
                        }
                    }

                    constexpr bool as_qvarint() {
                        if (is_varint) {
                            return true;
                        }
                        auto cpy = byts;
                        QVarInt val{};
                        if (!val.parse(cpy) || cpy.len) {
                            return false;
                        }
                        qvint = val;
                        is_varint = true;
                        return true;
                    }

                    constexpr bool as_bytes() {
                        return !is_varint;
                    }

                    QVarInt qvarint() const {
                        if (!is_varint) {
                            return QVarInt(~0);
                        }
                        return qvint;
                    }

                    ByteLen bytes() const {
                        if (is_varint) {
                            return {};
                        }
                        return byts;
                    }
                };

                struct TransportParameter {
                    QVarInt id;
                    QVarInt len;  // only parse
                    TransportParamValue data;

                    constexpr bool parse(ByteLen& b) {
                        if (!id.parse(b) ||
                            !len.parse(b)) {
                            return false;
                        }
                        return data.parse(b, len);
                    }

                    constexpr size_t parse_len() const {
                        return id.len + len.len + data.parse_len();
                    }

                    constexpr bool render(WPacket& w) const {
                        if (!id.render(w)) {
                            return false;
                        }
                        if (!QVarInt(data.render_len()).render(w)) {
                            return false;
                        }
                        return data.render(w);
                    }

                    constexpr size_t render_len() const {
                        return id.minimum_len() + QVarInt(data.render_len()).minimum_len() + data.render_len();
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
                    max_datagram_frame_size = 0x20,  // RFC 9221 - An Unreliable Datagram Extension to QUIC
                    grease_quic_bit = 0x2ab2,        // RFC 9287 - Greasing the QUIC Bit
                };

                constexpr auto implemented_defined = 19;

                struct PreferredAddress {
                    byte ipv4_address[4]{};
                    std::uint16_t ipv4_port = 0;
                    byte ipv6_address[16]{};
                    std::uint16_t ipv6_port{};
                    byte connectionID_length;  // only parse
                    ByteLen connectionID;
                    byte stateless_reset_token[16];

                    constexpr bool parse(ByteLen& b) {
                        if (view::copy(ipv4_address, view::rvec(b.data, b.len)) < 0) {
                            return false;
                        }
                        b = b.forward(4);
                        if (!b.as(ipv4_port)) {
                            return false;
                        }
                        b = b.forward(2);
                        if (view::copy(ipv6_address, view::rvec(b.data, b.len)) < 0) {
                            return false;
                        }
                        b = b.forward(16);
                        if (!b.as(ipv6_port)) {
                            return false;
                        }
                        b = b.forward(2);
                        if (!b.enough(1)) {
                            return false;
                        }
                        connectionID_length = *b.data;
                        if (!b.fwdresize(connectionID, 1, connectionID_length)) {
                            return false;
                        }
                        b = b.forward(connectionID_length);
                        if (view::copy(stateless_reset_token, view::rvec(b.data, b.len)) < 0) {
                            return false;
                        }
                        b = b.forward(16);
                        return true;
                    }

                    constexpr size_t param_len() const {
                        return 4 + 2 + 16 + 2 +
                               1 +  // length
                               connectionID.len + 16;
                    }

                    constexpr bool render(WPacket& w) const {
                        w.append(ipv4_address, 4);
                        w.as(ipv4_port);
                        w.append(ipv6_address, 16);
                        w.as(ipv6_port, 2);
                        if (connectionID.len > 0xff) {
                            return false;
                        }
                        if (!connectionID.data && connectionID.len) {
                            return false;
                        }
                        w.add(byte(connectionID.len), 1);
                        w.append(connectionID.data, connectionID.len);
                        w.append(stateless_reset_token, 16);
                        return true;
                    }
                };

                constexpr TransportParameter make_transport_param(DefinedTransportParamID id, ByteLen data) {
                    return {size_t(id), size_t(), data};
                }

                constexpr TransportParameter make_transport_param(DefinedTransportParamID id, std::uint64_t data) {
                    return {size_t(id), size_t(), data};
                }

                struct DuplicateSetChecker {
                    bool check[implemented_defined] = {};
                    bool detect = false;
                };

                struct DefinedTransportParams {
                    ByteLen original_dst_connection_id;
                    std::uint64_t max_idle_timeout = 0;
                    ByteLen stateless_reset_token;
                    std::uint64_t max_udp_payload_size = 65527;
                    std::uint64_t initial_max_data = 0;
                    std::uint64_t initial_max_stream_data_bidi_local = 0;
                    std::uint64_t initial_max_stream_data_bidi_remote = 0;
                    std::uint64_t initial_max_stream_data_uni = 0;
                    std::uint64_t initial_max_streams_bidi = 0;
                    std::uint64_t initial_max_streams_uni = 0;
                    std::uint64_t ack_delay_exponent = 3;
                    std::uint64_t max_ack_delay = 25;
                    bool disable_active_migration = false;
                    PreferredAddress preferred_address;
                    std::uint64_t active_connection_id_limit = 2;
                    ByteLen initial_src_connection_id;
                    ByteLen retry_src_connection_id;
                    std::uint64_t max_datagram_frame_size = 0;
                    bool grease_quic_bit = false;

                    bool from_transport_param(TransportParameter param, DuplicateSetChecker* checker = nullptr) {
                        auto id = param.id.value;
                        auto index = 0;
                        auto check = [&] {
                            if (checker && checker->check[index]) {
                                checker->detect = true;
                                return false;
                            }
                            return true;
                        };
                        auto set = [&] {
                            if (checker) {
                                checker->check[index] = true;
                            }
                        };
#define VLINT(ID)                                                     \
    if (DefinedTransportParamID(id) == DefinedTransportParamID::ID) { \
        if (!check()) {                                               \
            return false;                                             \
        }                                                             \
        if (!param.data.as_qvarint()) {                               \
            return false;                                             \
        }                                                             \
        ID = param.data.qvarint();                                    \
        set();                                                        \
        return true;                                                  \
    }                                                                 \
    index++;
#define RDATA(ID)                                                     \
    if (DefinedTransportParamID(id) == DefinedTransportParamID::ID) { \
        if (!check()) {                                               \
            return false;                                             \
        }                                                             \
        if (!param.data.as_bytes()) {                                 \
            return false;                                             \
        }                                                             \
        ID = param.data.bytes();                                      \
        set();                                                        \
        return true;                                                  \
    }                                                                 \
    index++;
#define BOOL(ID)                                                      \
    if (DefinedTransportParamID(id) == DefinedTransportParamID::ID) { \
        if (!check()) {                                               \
            return false;                                             \
        }                                                             \
        if (param.data.parse_len() != 0) {                            \
            return false;                                             \
        }                                                             \
        ID = true;                                                    \
        set();                                                        \
        return true;                                                  \
    }                                                                 \
    index++;

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
                        BOOL(disable_active_migration)
                        if (DefinedTransportParamID(id) == DefinedTransportParamID::preferred_address) {
                            if (!check()) {
                                return false;
                            }
                            if (!param.data.as_bytes()) {
                                return false;
                            }
                            auto copy = param.data.bytes();
                            if (!preferred_address.parse(copy)) {
                                return false;
                            }
                            set();
                            return true;
                        }
                        index++;
                        VLINT(active_connection_id_limit);
                        RDATA(initial_src_connection_id);
                        RDATA(retry_src_connection_id);
                        VLINT(max_datagram_frame_size);
                        BOOL(grease_quic_bit)
                        return true;  // not matched but no error
#undef RDATA
#undef VLINT
#undef BOOL
                    }

                    template <class Array = easy::array<std::pair<TransportParameter, bool>, implemented_defined>>
                    Array to_transport_param(WPacket& src) {
                        Array params{};
                        auto index = 0;
#define MAKE(param)                                                             \
    {                                                                           \
        auto tmp = make_transport_param(DefinedTransportParamID::param, param); \
        params[index] = {tmp, true};                                            \
    }                                                                           \
    index++;
#define BOOL(param)                                                                               \
    if (param) {                                                                                  \
        params[index] = {make_transport_param(DefinedTransportParamID::param, ByteLen{}), true};  \
    }                                                                                             \
    else {                                                                                        \
        params[index] = {make_transport_param(DefinedTransportParamID::param, ByteLen{}), false}; \
    }                                                                                             \
    index++;
                        MAKE(original_dst_connection_id);
                        MAKE(max_idle_timeout);
                        MAKE(stateless_reset_token);
                        MAKE(max_udp_payload_size);
                        MAKE(initial_max_data);
                        MAKE(initial_max_stream_data_bidi_local);
                        MAKE(initial_max_stream_data_bidi_remote);
                        MAKE(initial_max_stream_data_uni);
                        MAKE(initial_max_streams_bidi);
                        MAKE(initial_max_streams_uni);
                        MAKE(ack_delay_exponent);
                        MAKE(max_ack_delay);
                        BOOL(disable_active_migration);
                        WPacket tmp;
                        tmp.b = src.acquire(preferred_address.param_len());
                        if (preferred_address.render(tmp)) {
                            params[index] = {make_transport_param(DefinedTransportParamID::preferred_address, tmp.b.resized(tmp.offset)), true};
                        }
                        else {
                            params[index] = {make_transport_param(DefinedTransportParamID::preferred_address, ByteLen{}), false};
                        }
                        index++;
                        MAKE(active_connection_id_limit);
                        MAKE(initial_src_connection_id);
                        MAKE(retry_src_connection_id);
                        MAKE(max_datagram_frame_size);
                        BOOL(grease_quic_bit)
#undef MAKE
#undef BOOL
                        return params;
                    }
                };

                constexpr bool is_defined_both_set_allowed(std::uint64_t id_raw) {
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
                           id == DefinedTransportParamID::max_datagram_frame_size ||
                           id == DefinedTransportParamID::grease_quic_bit;
                }

                constexpr bool is_defined_server_set_allowed(std::uint64_t id_raw) {
                    const auto id = DefinedTransportParamID(id_raw);
                    return is_defined_both_set_allowed(id_raw) ||
                           id == DefinedTransportParamID::original_dst_connection_id ||
                           id == DefinedTransportParamID::preferred_address ||
                           id == DefinedTransportParamID::stateless_reset_token ||
                           id == DefinedTransportParamID::retry_src_connection_id;
                }

                constexpr bool is_defined(std::uint64_t id_raw) {
                    return is_defined_server_set_allowed(id_raw);
                }

                enum class TransportParamError {
                    none,
                    invalid_encoding,
                    less_than_2_connection_id,
                    less_than_1200_udp_payload,
                    over_20_ack_delay_exponent,
                    over_2pow14_max_ack_delay,
                    zero_length_connid,
                    too_large_max_streams_bidi,
                    too_large_max_streams_uni,
                };

                constexpr const char* to_string(TransportParamError err) {
                    switch (err) {
                        case TransportParamError::invalid_encoding:
                            return "invalid transport parameter encoding";
                        case TransportParamError::less_than_2_connection_id:
                            return "active_connection_id_limit is less than 2";
                        case TransportParamError::less_than_1200_udp_payload:
                            return "max_udp_payload_size is less than 1200";
                        case TransportParamError::over_20_ack_delay_exponent:
                            return "ack_delay_exponent is over 20";
                        case TransportParamError::zero_length_connid:
                            return "preferred_address.connectionID.len is 0";
                        case TransportParamError::too_large_max_streams_bidi:
                            return "max_bidi_streams greater than 2^60";
                        case TransportParamError::too_large_max_streams_uni:
                            return "max_uni_streams greater than 2^60";
                        default:
                            return nullptr;
                    }
                }

                constexpr TransportParamError validate_transport_param(DefinedTransportParams& param) {
                    if (param.ack_delay_exponent > 20) {
                        return TransportParamError::over_20_ack_delay_exponent;
                    }
                    if (param.active_connection_id_limit < 2) {
                        return TransportParamError::less_than_2_connection_id;
                    }
                    if (param.max_udp_payload_size < 1200) {
                        return TransportParamError::less_than_1200_udp_payload;
                    }
                    if (param.max_ack_delay > (1 << 14)) {
                        return TransportParamError::over_2pow14_max_ack_delay;
                    }
                    if (param.preferred_address.connectionID.data && param.preferred_address.connectionID.len < 1) {
                        return TransportParamError::zero_length_connid;
                    }
                    if (param.initial_max_streams_bidi >= size_t(1) << 60) {
                        return TransportParamError::too_large_max_streams_bidi;
                    }
                    if (param.initial_max_streams_uni >= size_t(1) << 60) {
                        return TransportParamError::too_large_max_streams_uni;
                    }
                    return TransportParamError::none;
                }

                // TransportParamBox is boxing utility of DefinedTransportParameter
                struct TransportParamBox {
                    BoxByteLen boxed[5];

                   private:
                    bool do_boxing(BoxByteLen& box, ByteLen& param) {
                        if (param.valid() && box.data() != param.data) {
                            box = param;
                            if (!box.valid()) {
                                return false;
                            }
                            param = box.unbox();
                        }
                        return true;
                    }

                   public:
                    bool boxing(DefinedTransportParams& params) {
                        return do_boxing(boxed[0], params.original_dst_connection_id) &&
                               do_boxing(boxed[1], params.stateless_reset_token) &&
                               do_boxing(boxed[2], params.preferred_address.connectionID) &&
                               do_boxing(boxed[3], params.initial_src_connection_id) &&
                               do_boxing(boxed[4], params.retry_src_connection_id);
                    }
                };

            }  // namespace trsparam
        }      // namespace quic
    }          // namespace dnet
}  // namespace utils

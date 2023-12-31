/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// defined_id - defined transport parameter id
#pragma once
#include <cstdint>

namespace utils {
    namespace fnet::quic::trsparam {
        enum class DefinedID {
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

        constexpr const char* to_string(DefinedID id) noexcept {
#define MAP(code)         \
    case DefinedID::code: \
        return #code;
            switch (id) {
                MAP(original_dst_connection_id)
                MAP(max_idle_timeout)
                MAP(stateless_reset_token)
                MAP(max_udp_payload_size)
                MAP(initial_max_data)
                MAP(initial_max_stream_data_bidi_local)
                MAP(initial_max_stream_data_bidi_remote)
                MAP(initial_max_stream_data_uni)
                MAP(initial_max_streams_bidi)
                MAP(initial_max_streams_uni)
                MAP(ack_delay_exponent)
                MAP(max_ack_delay)
                MAP(disable_active_migration)
                MAP(preferred_address)
                MAP(active_connection_id_limit)
                MAP(initial_src_connection_id)
                MAP(retry_src_connection_id)
                MAP(max_datagram_frame_size)
                MAP(grease_quic_bit)
                default:
                    return nullptr;
            }
#undef MAP
        }

        constexpr auto implemented_defined = 19;

        constexpr bool is_defined_both_set_allowed(std::uint64_t id_raw) {
            const auto id = DefinedID(id_raw);
            return id == DefinedID::max_idle_timeout ||
                   id == DefinedID::max_udp_payload_size ||
                   id == DefinedID::initial_max_data ||
                   id == DefinedID::initial_max_stream_data_bidi_local ||
                   id == DefinedID::initial_max_stream_data_bidi_remote ||
                   id == DefinedID::initial_max_stream_data_uni ||
                   id == DefinedID::initial_max_streams_bidi ||
                   id == DefinedID::initial_max_streams_uni ||
                   id == DefinedID::ack_delay_exponent ||
                   id == DefinedID::max_ack_delay ||
                   id == DefinedID::disable_active_migration ||
                   id == DefinedID::active_connection_id_limit ||
                   id == DefinedID::initial_src_connection_id ||
                   id == DefinedID::max_datagram_frame_size ||
                   id == DefinedID::grease_quic_bit;
        }

        constexpr bool is_defined_server_set_allowed(std::uint64_t id_raw) {
            const auto id = DefinedID(id_raw);
            return is_defined_both_set_allowed(id_raw) ||
                   id == DefinedID::original_dst_connection_id ||
                   id == DefinedID::preferred_address ||
                   id == DefinedID::stateless_reset_token ||
                   id == DefinedID::retry_src_connection_id;
        }

        constexpr bool is_defined(std::uint64_t id_raw) {
            return is_defined_server_set_allowed(id_raw);
        }

    }  // namespace fnet::quic::trsparam
}  // namespace utils
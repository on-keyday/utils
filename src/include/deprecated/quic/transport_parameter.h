/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// transport_parameter - transport parameter
#pragma once

namespace utils {
    namespace quic {
        // transport parameter
        namespace tpparam {

            /*
            A client MUST NOT use remembered values for the following parameters:
            ack_delay_exponent, max_ack_delay, initial_source_connection_id,
            original_destination_connection_id, preferred_address, retry_source_connection_id,
            and stateless_reset_token.
            The client MUST use the server's new values in the handshake instead;
            if the server does not provide new values, the default values are used.

            a server that accepts 0-RTT data MUST NOT set values for the following parameters (Section 18.2) that are smaller than the remembered values of the parameters.

            * active_connection_id_limit

            * active_connection_id_limit

            * initial_max_data

            * initial_max_data

            * initial_max_stream_data_bidi_local

            * initial_max_stream_data_bidi_local

            * initial_max_stream_data_bidi_remote

            * initial_max_stream_data_bidi_remote

            * initial_max_stream_data_uni

            * initial_max_stream_data_uni

            * initial_max_streams_bidi

            * initial_max_streams_bidi

            * initial_max_streams_uni

            The applicable subset of transport parameters
            that permit the sending of application data
            SHOULD be set to non-zero values for 0-RTT.
            (訳:アプリケーションデータの送信を許可するトランスポートパラメーターの
            　　該当する部分集合は0-RTTのために0以外の値に設定されるべきです)
            This includes initial_max_data and either
            (1) initial_max_streams_bidi and initial_max_stream_data_bidi_remote or
            (2) initial_max_streams_uni and initial_max_stream_data_uni.
            A server MAY store and recover the previously sent values of
            the max_idle_timeout, max_udp_payload_size,
            and disable_active_migration parameters
            and reject 0-RTT if it selects smaller values.
            A server MUST reject 0-RTT data if the restored values for transport parameters cannot be supported.

            When sending frames in 0-RTT packets,
            a client MUST only use remembered transport parameters;
            importantly, it MUST NOT use updated values that it learns
            from the server's updated transport parameters
            or from frames received in 1-RTT packets.
            Updated values of transport parameters from the handshake apply
            only to 1-RTT packets.
            For instance, flow control limits from remembered transport parameters
            apply to all 0-RTT packets even if those values are increased
            by the handshake or by frames sent in 1-RTT packets.
            A server MAY treat the use of updated transport parameters
            in 0-RTT as a connection error of type PROTOCOL_VIOLATION.
            */
            struct tag_section_7_4_1;

            /*
            Implementations MUST support buffering at least 4096 bytes
            of data received in out-of-order CRYPTO frames.

            Once the handshake completes,
            if an endpoint is unable to buffer all data in a CRYPTO frame,
            it MAY discard that CRYPTO frame and all CRYPTO frames received
            in the future, or it MAY close the connection
            with a CRYPTO_BUFFER_EXCEEDED error code.
            Packets containing discarded CRYPTO frames MUST be acknowledged
            because the packet has been received
            and processed by the transport even though the CRYPTO frame
            was discarded.
            */
            struct tag_section_7_5;
        }  // namespace tpparam
    }      // namespace quic
}  // namespace utils

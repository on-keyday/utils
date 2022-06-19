/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic_doc - quic rfc9000 document reading
#pragma once
#include <cstdint>

namespace utils {
    namespace quic {

        // sending api
        // - write data
        //     understanding when
        //     stream flow control credit (Section 4.1)
        //     has successfully been reserved to send the written data;
        using tag_write_data_section_4_1 = void;
        // - end stream with graceful shutdown
        //     resulting in a STREAM frame (Section 19.8) with the FIN bit set
        using tag_end_stream_clean_section_19_8 = void;
        // - reset stream
        //     resulting in a RESET_STREAM frame (Section 19.4) if the stream was not already in a terminal state.
        using tag_end_stream_abort_section_19_4 = void;

        // reading api
        // - read data
        // - abort reading
        //     resulting in a STOP_SENDING frame (Section 19.5).
        using tag_abort_reading_section_19_5 = void;

        namespace frame {
            // sender MUST NOT send STREAM, STREAM_DATA_BLOCKED and RESET_STREAM at ResetRecved or DataRecved
            // sender MUST NOT send at ResetRecved STREAM, STREAM_DATA_BLOCKED
            // reciever could recv STREAM, STREAM_DATA_BLOCKED and RESET_STREM on any state
            // reciever sends MAX_STREAM_DATA and STOP_SENDING
            // reciever can send only MAX_STREAM_DATA
            // reciever MAY send STOP_SENDING in any states without ResetRecv and ResetRead
            // there is little value (訳:価値) to send STOP_SENDING at RecvData because all data has recieved
            enum class types {
                STREAM,               // section 19.8
                STREAM_DATA_BLOCKED,  // section 19.13
                RESET_STREAM,         // section 19.4
                //  related: stream flow control credit (Section 4.1)
                MAX_STREM_DATA,  // section 19.10
                STOP_SENDING,    // section 19.5
            };

            namespace bits {
                enum {
                    FIN,
                    ACK,
                };
            }
        }  // namespace frame

        // send stream state
        namespace stream {

            enum class send_state_bidirectional {
                // application making object of sending
                CreateStreamSending,
                NoStream,
                // ready to send
                // Stream data might be buffered in this state in preparation for sending.
                // The sending part of a bidirectional stream initiated by a peer
                // (type 0 for a server, type 1 for a client) starts in the "Ready"
                // state when the receiving part is created.
                Ready,
                // STREAM or STREAM_DATA_BLOCKED sent
                // An implementation might choose to defer allocating a stream ID
                // to a stream until it sends the first STREAM frame and enters this state
                Send,
                // STREAM + FIN sent
                // The endpoint does not need to check flow control limits or
                // send STREAM_DATA_BLOCKED frames for a stream in this state.
                DataSent,
                // Recv All ACKs
                DataRecved,
                ResetSent,
                ResetRecved,
            };

            enum class recv_state_bidirectional {
                NoStream,
                // Recv STREAM / STREAM_DATA_BLOCKED / RESET_STREAM create Bidirectional Stream (Sending)
                // Create Bidirectional Stream (Sending) create Higher-Numbered Stream
                Recv,  // == Ready ~ Send
                // Recv STREAM + FIN
                // see Section 4.5
                SizeKnown,  // == DataSent ~ DataRecved
                // This might happen as a result of receiving
                // the same STREAM frame that causes the transition to "Size Known".
                // After all data has been received, any STREAM or STREAM_DATA_BLOCKED frames
                // for the stream can be discarded.
                DataRecved,
                DataRead,
                // Recv RESET_STREAM
                ResetRecved,
                ResetRead,
            };

            enum class http2_state {
                idle,
                open,
                half_closed_remote,
                half_closed_local,
                closed,
            };

            struct StateMapping {
                send_state_bidirectional sender[3];
                recv_state_bidirectional reciever[2];
                http2_state http2;
            };

            // section 3.4 Bidirectional Stream States
            StateMapping state_mapping[] = {
                {{send_state_bidirectional::NoStream, send_state_bidirectional::Ready},
                 {recv_state_bidirectional::NoStream, recv_state_bidirectional::Recv},
                 http2_state::idle},
                {{send_state_bidirectional::Ready, send_state_bidirectional::Send, send_state_bidirectional::DataSent},
                 {recv_state_bidirectional::Recv, recv_state_bidirectional::SizeKnown},
                 http2_state::open},
                {{send_state_bidirectional::Ready, send_state_bidirectional::Send, send_state_bidirectional::DataSent},
                 {recv_state_bidirectional::DataRecved, recv_state_bidirectional::DataRead},
                 http2_state::half_closed_remote},
                {{send_state_bidirectional::Ready, send_state_bidirectional::Send, send_state_bidirectional::DataSent},
                 {recv_state_bidirectional::ResetRecved, recv_state_bidirectional::ResetRead},
                 http2_state::half_closed_remote},
                {{send_state_bidirectional::DataRecved},
                 {recv_state_bidirectional::Recv, recv_state_bidirectional::SizeKnown},
                 http2_state::half_closed_local},
                {{send_state_bidirectional::ResetSent, send_state_bidirectional::ResetRecved},
                 {recv_state_bidirectional::Recv, recv_state_bidirectional::SizeKnown},
                 http2_state::half_closed_local},
                {{send_state_bidirectional::ResetSent, send_state_bidirectional::ResetRecved},
                 {recv_state_bidirectional::DataRecved, recv_state_bidirectional::DataRead},
                 http2_state::closed},
                {{send_state_bidirectional::ResetSent, send_state_bidirectional::ResetRecved},
                 {recv_state_bidirectional::ResetRecved, recv_state_bidirectional::ResetRead},
                 http2_state::closed},
                {{send_state_bidirectional::DataRecved},
                 {recv_state_bidirectional::DataRecved, recv_state_bidirectional::DataRead},
                 http2_state::closed},
                {{send_state_bidirectional::DataRecved},
                 {recv_state_bidirectional::ResetRecved, recv_state_bidirectional::ResetRead},
                 http2_state::closed},
            };

            // 3.5 Solicited State Trnsitions
            // definition
            //   sender - endpoint which plays role of section 3.1 Sending Stream States
            //   reciver - endpoint which plays role of section 3.2 Receving Stream States
            // - if reciever wants to abort reading at Recv or SizeKnown state,
            //   reciever SHOULD send STOP_SENDING frame for sender to stop and close stream.
            //   reciever can select behaviour to already received data; ignore or accept (ignore is recommennded).
            // - STREAM frames after STOP_SENDING sent still affect to "connection and stream's flow control";
            //   reciever can discard these frames' data but cannot ignore size.
            // - STOP_SENDING frame requests sender to send RESET_STREAM frame.
            //   sender MUST send RESET_STREAM frame if state is in Ready or Send.
            //   sender MAY defer sending RESET_STREAM if state is in DataSent
            //   until sender ensure outstanding data acknowledged or lost.
            //   if these are ensured, sender SHOULD send RESET_STREAM frame instead retransmitting data.
            // - sender SHOULD copy error code from STOP_SENDING to RESET_STREAM sender sends,
            //   but sender can use any application error code which not provided from STOP_SENDING frame.
            //   receiver MAY ignore error code in any RESET_STREAM frames after STOP_SENDING sent
            // - STOP_SENDING SHOULD be sent for stream that has not been reset by sender;
            //   stream is already in Recv or SizeKnown state
            // - if packets containing STOP_SENDING frame is lost, reciever is expected to send another STOP_SENDING,
            //   but if reciver recieved all data or RESET_STREAM frame for the stream by the time reciver detect STOP_SENDING is lost,
            //   state is already not in Recv or SizeKnown and so receiver needn't send STOP_SENDING.
            // summary
            // - sender can terminate stream by sending RESET_STREAM
            // - receiver can prompt termination to sender by sending STOP_SENDING frame
            struct tag_section_3_5;

        }  // namespace stream

        using byte = std::uint8_t;
        namespace frame {
            namespace id {
                //                client    | server
                // bidirectional |  0x1 00  |  0x2 01
                // unidirectional|  0x2 10  |  0x3 11
                constexpr byte client_bidi = 0x0;
                constexpr byte server_bidi = 0x1;
                constexpr byte client_unid = 0x2;
                constexpr byte server_unid = 0x3;
            }  // namespace id
        }      // namespace frame
    }          // namespace quic
}  // namespace utils

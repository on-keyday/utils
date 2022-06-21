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
                DATA_BLOCKED,
                RESET_STREAM,  // section 19.4
                //  related: stream flow control credit (Section 4.1)
                MAX_DATA,        // section 19.9
                MAX_STREM_DATA,  // section 19.10
                STOP_SENDING,    // section 19.5
                MAX_STREAMS,     // section 18.2
                ACK,
                STREAM_BLOCKED,  // section 19.14
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
            constexpr StateMapping state_mapping[] = {
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
            //    sender - endpoint which plays role of section 3.1 Sending Stream States
            //    reciver - endpoint which plays role of section 3.2 Receving Stream States
            // notes
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

            // 4 Flow Control
            // definition
            //    receiver - endpoint receives data
            //    sender - endpoint sends data
            //    endpoint - both reciver adn sender
            // notes
            // - receivers need to limit amount of data to prevent attack
            //   from fast sender with very large data or sender which makes receiver use large memory
            // - To enable recivers to limit memory usage for connection,
            //   streams are flow controlled both individually and whole connection.
            //   QUIC receivers controls maximum amount of data sender can send on a stream
            //   across all streams at any time (described at section 4.1 and 4.2)
            // - To limit concarrency within connection, QUIC endpoint controls
            //   maximum cumulative(訳:累積的な) number of streams endpoints can initiate
            //   (described at section 4.6)
            // - CRYPTO frames is not flow controlled in the same way as stream data
            //   QUIC depends cryptographic protocols (TLS) to avoid using large memory
            //   QUIC SHOULD provide interface to communicate buffering limits
            //   (described at QUIC-TLS rfc9001)
            struct tag_section_4_0;

            // 4.1 Data Flow Control
            // notes
            // - QUIC uses limit-based flow control scheme
            //   where a receiver advertises the limit of total bytes which is prepared to receive
            // - QUIC has two levels of data flow control
            //   Stream flow control: preventing a single stream from consuming all connection buffer
            //   Connection flow control: preventing sender from exceeding(訳:超える)
            //                            receivers buffer capacity for the connection
            //                            by limiting the total bytes of stream data sent in STREAM frames
            //                            on all streams
            // - senders MUST NOT send data excess of either limit
            // - receiver sets initial limits for all streams through transport parameters
            //   during handshake (section 7.4). after that, a receiver sends
            //   MAX_STREAM_DATA (section 19.10) or MAX_DATA (section 19.9) to sender advertise larger limits
            // - receiver advertise a larger limit for a stream by MAX_STREAM_DATA which contains stream ID
            //   MAX_STREAM_DATA indicates the maximum absolute byte offset of a stream
            //   receiver can determine the offset to be advertised based on the current offset of data
            //   consumed by sender on the stream
            // - receiver advertise a larger limit for a connection by MAX_DATA
            //   MAX_DATA indicate the maximum of the sum of the aboslute byte offset of all streams
            // - receiver maintains a cumulative sum of bytes received of all streams to check vaiolaction of limit for senders
            // - receiver can determine the maximum data limit to be advertised based on the sum of bytes consumed
            //   by sender on all streams.
            // - after larger limit advertised, smaller limits has no effect to sender
            // - receiver MUST close connection with FLOW_CONTROL_ERROR if sender violates limits of data of stream or connection
            // - sender MUST ignore any MAX_STREAM_DATA or MAX_DATA not increase flow control limits
            // - if sender reaches limit; sender couldn't send new data and is considered blocked,
            //   sender SHOULD send a STREAM_DATA_BLOCKED or DATA_BLOCKED to indicate to receiver
            //   that sender has more data to send but blocked by flow control limits.
            //   if sender is blocked for a period longer than idle timeout (section 10.1)
            //   receiver might close the connection even when sender has data to send.
            //   To keep connection available, sender blocked by flow control limit
            //   SHOULD send STREAM_DATA_BLOCKED or DATA_BLOCKED when sender has no ACK-eliciting packets
            struct tag_section_4_1;

            // 4.2 Increasing Flow Control Limits
            // - To avoid blocking sender, reciver MAY send MAX_STREAM_DATA or MAX_DATA
            //   multiple in a roundtrip and earier than sender cosumes limits.
            // - Control frames contribute to connection overhead.
            //   So sending MAX_STREAM_DATA and MAX_DATA with small increments is undesirable
            //   On the other hand, larger increments requires large memory resource
            //   There is trade-off betweeen resource commitment and overhead
            // - receiver MUST NOT wait for STREAM_DATA_BLOCKED or DATA_BLOCKED
            //   before sending MAX_STREAM_DATA and MAX_DATA;
            //   waiting could result in sender being blocked for the rest of the connection.
            //   Even if sender sends these frames, waiting for them will result in sender being blocked for at least
            //   an entire round trip (訳:STREAM_DATA_BLOCKEDなどを待機すると送信者がブロックされるので待機するべきでない)
            // - When sender received credit after being blocked, sender might be able to send
            //   a large amount of data, resulting in short-term congestion(訳:輻輳)
            //   see [section 7.7 of QUIC_RECOVERY rfc9002] for a discussion how to avoid congestion
            struct tag_section_4_2;

            // 4.3 Flow Control performance
            // definition
            //   peer - an other side endpoint
            // notes
            // - if endpoint cannot ensure peer always has available flow control
            //   credit greater than peer's bandwidth-delay product on connection,
            //   endpoint's receive throughput will limited by flow control
            // - packet loss can causes gap in the recieve buffer, preventing the application
            //   from consuming data and freeing up receive buffer space
            // - Sending timely update of flow control limits can improve performance
            //   but sending packets only to provide flow control updates can increase
            //   network load and adversely affects performance.
            //   So send flow control updates with other frames, such as ACK frame,
            //   to reduce the cost of updates
            struct tag_section_4_3;

            // 4.4 Handling Stream Cancellation
            // - sender and receiver need to finally agreee on amount of flow control credit
            //   consumed on every stream, in order to be able to accounts for all bytes for connection level flow control
            // - when receiver received RESET_STREAM, receiver will change state (to ResetRecved or ResetRead)
            //   and ignore futher data arriving on that stream
            // - RESET_STREAM terminates one direction of stream. if stream is bidirectional stream,
            //   opposite direction of the stream has still opened.
            //   sender and receiver MUST maintain flow control for the stream in the unterminated direction until
            //   that direction enters a terminal state.
            struct tag_section_4_4;

            // 4.5 Stream Final Size
            // - The final size is the amount of flow control credit that is consumed by a stream
            // - Assuming every contigus byte on the stream was sent once,
            //   the final size is the number of bytes sent.
            //   More generally, this is one higher than the offset of the byte with the largest offset
            //   sent on the stream, or zero if no bytes were sent
            // - sender always communicates the final size of a stream to the receiver reliably
            //   no matter how the stream is terminated.
            //   The final size is the sum of the Offset and Length fields of STREAM frame with FIN flag
            //   which may be omitted field.
            //   Alternatively, the FinalSize field of RESET_STREAM frame carries this value
            //   This garantees that sender and receiver agree on how much flow control credit was consumed by sender on that stream
            // - receiver will know the final size when STREAM+FIN or RESET_STREAM is received and the stream enters SizeKnown or ResetRecved
            //   receiver MUST use the final size of the stream to account for all bytes sent in connection level flow contoller.
            // - sender MUST NOT send data on a stream at or beyond the final size
            // - Once final size is known, it cannot change.
            //   If RESET_STREAM or STREAM frame which change final size recieved after receiver having received final size,
            //   receiver SHOULD respond with FINAL_SIZE_ERROR even after a stream is closed.
            //   Generating these error is not mandatory because requireing this means the endpoint
            //   need to maintain the final size state for closed streams, which is hard for recource commitment.
            struct tag_section_4_5;

            // 4.6 Controlling Concurrency
            // - An endpoint limits the cumulative number of incoming streams a peer can open.
            //   Only streams with a stream ID less than (max_streams * 4 + first_stream_id_of_type) can be opened;
            //   See Table 1. Initial limits are set in the transport parameters;
            //   See Section 18.2 Subsequent limits are advertised using MAX_STREAMS;
            //   See Section 19.11 Separate limits appply to unidirectional and bidirectional streams
            // - if max_streams transport parameter or MAX_STREAMS frame received with a value grater than 2^60
            //   this would allow a maximum stream ID that cannot be expressed as a variable-length integer;see Section 16.
            //   If either is recived, the connection MUST be closed immediately
            //   with TRANSPORT_PARAMETER_ERROR if offerring value was received in a transport parameter
            //   or with FRAME_ENCODING_ERROR if it was received in frame; see Section 10.2
            // - Endpoints MUST NOT exceed the limit set by their peer.
            //   Endpoint receives a frame with a stream ID exceeeding the limit it has sent
            //   MUST treat this as a connection error STREAM_LIMIT_ERROR; see Section 11
            // - Once a receiver advertises a stream limit using the MAX_STREAMS frame,
            //   advertising a smaller limit has no effect. MAX_STREAMS frames that do not increase the stream limit
            //   MUST be ignored
            // - Implementations might choose to increase limits as streams are closed,
            //   to keep the number of streams available to peers roughly consistent(訳:一貫した)
            // - Endpoint that is unable to open a new stream due to the peer's limits
            //   SHOULD send a STREAM_BLOCKED frmaes (section 19.14).
            //   This signal is considered useful for debugging.
            //   An endpoint MUST NOT wait to receive this signal before advertising addtional credit,
            //   since doing so will mean that the peer will be blocked for at least an entire round trip,
            //   and potentially idefinitely if the peer chooses not to send STREAMS_BLOCKED frames.
            struct tag_section_4_6;

            enum class error_code {
                FLOW_CONTROL_ERROR,
                FINAL_SIZE_ERROR,
                TRANSPORT_PARAMETER_ERROR,  // section 10.2
                FRAME_ENCODING_ERROR,       // section 10.2
                STREAM_LIMIT_ERROR,         // section 11
            };

        }  // namespace stream

        namespace connect {

            // 5. Connection
            // - A QUIC connection is shared state between a client and a server
            // - Each connection starts with a handshake phase,
            //   during which the two endpoints establish a shared secret using the cryptographic handshake
            //   protocol [QUIC-TLS] and negotiate the application protocol.
            //   The handshake (section 7.0) confirms that both endpoints are willing to communicate (section 8.1)
            //   and establishes parameters for the connection (section 7.4)
            // - An application protocol can use the connection during the handshake phase
            //   with some limitations.
            //   0-RTT allows application data to be sent by a client before receiving a response from the server.
            //   However, 0-RTT provides no protection against replay attacks;see Section 9.2 of [QUIC-TLS]
            //   A server can also send application data to a client before if receives the final cryptographic handshake
            //   messages that allow it to confirm the identity and liveness of the client. Theese capabilities
            //   allow an application protocol to offer the option of trading some security guarantees for reduced latency.
            // - The use of connection IDs (section 5.1) allows connections to migrate to a new network path,
            //   both as a direct choice of an endpoint and when forced by a change in middlebox.
            //   Section 9 describe mitigations for the security and privacy issues associated with migration
            // - For connections that are no longer needed or desired, there are several ways for a client and server
            //   to terminate a connection, as described in Section 10
            struct tag_section_5_0;

            // 5.1 Connection ID
            // - Each connection prosesses(訳:所有している) a set of connection identifiers, or
            //   connection IDs, each of which can identify the connection.
            // - Connection IDs are independently selected by endpoints; each endpoint selects the connection
            //   IDs that its peer uses
            // - The primary function of a connection ID is to ensure that changes in addressing at lower protocol layers (UDP,IP)
            //   do not cause packets for a QUIC connection to be delivered to the wrong endpoint.
            //   Each endpoint selects connection IDs using an implementation-sepecific (and perhaps deployment-specific)
            //   method that will allow packets with that connection ID to be routed back to the endpoint and to be identified by
            //   the endpoint upon receipt.
            // - Multiple connection IDs are used so that endpoints can send packets that cannot be indentified
            //   by an observer as being for the same connection without cooperation from an endpoint;see Section 9.5
            // - Connection IDs MUST NOT contains any information that can be used by an external observer
            //   (that(訳:observer) is, one that does not cooperate with the issuer)
            //   to correlate(訳:相関する) them with other connection IDs for the same connection
            //   (訳:コネクションIDは同じコネクションで外部の観察者によって他のコネクションIDから関連づけられるいかなる情報も含むべきではありません。
            //       (同じIDの使いまわし連番のID等))
            // - Packets with long headers include SourceConnectionID and DestinationConnectionID fields.
            //   These
            struct tag_section_5_1;

        }  // namespace connect

        using byte = std::uint8_t;
        using tsize = std::uint64_t;
        namespace config {
            struct TransportParameter {
                tsize max_streams;
                tsize first_stream_id_of_type;
                bool can_open(tsize id) const {
                    return id < max_streams * 4 + first_stream_id_of_type;
                }
            };
        }  // namespace config

        namespace frame {
            namespace id {
                //                   client  |  server
                // bidirectional  |  0x1 00  |  0x2 01
                // unidirectional |  0x2 10  |  0x3 11
                constexpr byte client_bidi = 0x0;
                constexpr byte server_bidi = 0x1;
                constexpr byte client_unid = 0x2;
                constexpr byte server_unid = 0x3;
            }  // namespace id

            namespace impl {
                struct Stream {
                    int Offset;
                    int Length;
                };
            }  // namespace impl
        }      // namespace frame
    }          // namespace quic
}  // namespace utils

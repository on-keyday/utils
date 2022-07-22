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
                PADDING,               // section 19.1
                PING,                  // section 19.2
                ACK,                   // section 19.3
                ACK_ECN,               // with ECN count
                RESET_STREAM,          // section 19.4
                STOP_SENDING,          // section 19.5
                CRYPTO,                // section 19.6
                NEW_TOKEN,             // section 19.7
                STREAM,                // section 19.8
                MAX_DATA = 0x10,       // section 19.9
                MAX_STREAM_DATA,       // section 19.10
                MAX_STREAMS_BIDI,      // section 19.11
                MAX_STREAMS_UNI,       // section 19.11
                DATA_BLOCKED,          // section 19.12
                STREAM_DATA_BLOCKED,   // section 19.13
                STREAMS_BLOCKED_BIDI,  // section 19.14
                STREAMS_BLOCKED_UNI,   // section 19.14
                NEW_CONNECTION_ID,     // section 19.15
                RETIRE_CONNECTION_ID,  // seciton 19.16
                PATH_CHALLAGE,         // seciton 19.17
                PATH_RESPONSE,         // seciton 19.18
                CONNECTION_CLOSE,      // seciton 19.19
                CONNECTION_CLOSE_APP,  // application reason section 19.19
                HANDSHAKE_DONE,        // seciton 19.20
                DATAGRAM = 0x30,       // rfc 9221 datagram
                DATAGRAM_LEN,
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
            //   SHOULD send a STREAMS_BLOCKED frmaes (section 19.14).
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
                CONNECTION_ID_LIMIT_ERROR,
                CONNECTION_REFUSED,
                PROTOCOL_VIOLATION,
                CRYPTO_BUFFER_EXCEEDED,
            };

        }  // namespace stream

        namespace conn {

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
            //       (同じIDの使いまわし,連番のID等))
            // - Packets with long headers include SourceConnectionID and DestinationConnectionID fields.
            //   These field are used to set the connection IDs for new connections;see Section 7.2
            // - Packets with short header (Section 17.3) only include the DestinationConnectionID
            //   and omit explicit length. The length of the DestinatinConnectionID field is
            //   expected to be known to endpoints.
            //   Endpoints using a load balancer that routes based on connection ID
            //   could agreewith the load balancer on a fixed length for connection IDs or agree on an encoding scheme.
            //   A fixed portion could encode an explicit length, which allows the entire connection ID to vary in lenght
            //   and still be used by load balancer
            // - A Version Negotiation (Section 17.2.1) packet echoes the connection IDs
            //   selected by the client, both to ensure correct routing toward the client and
            //   to demonstrate that the packet is in reponse to a packet sent by the client.
            // - A zero-length connection ID can be used when a connection ID is not needed to route to the correct
            //   endpoint. However, multiplexing connections on the same local IP address and port while using zero-length
            //   connection IDs will cause failures in the presence of peer connection migration,
            //   NAT rebinding, and client port reuse. An endpoint MUST NOT use the smae IP address and port for
            //   multiple concurrent connections with zero-length connection IDs,
            //   unless it is certain that those protocol features are not in use
            // - When an endpoint uses a non-zero-length connection ID, it needs to ensure that the peer has
            //   a supply of connection IDs from which to choose for packets sent to the endpoint.
            //   These connection IDs are supplied by the endpoint using the NEW_CONNECTION_ID frame (Section 19.15)
            struct tag_section_5_1;

            // 5.1.1 Issuing Connection IDs
            // - Each connection ID has an associated sequence number to assit in
            //   detecting when NEW_CONNECTION_ID or RETIRE_CONNECTION_ID frames
            //   refer to the same value.
            // - The initial connection ID issued by an endpoint is sent in the SourceConnectionID field
            //   of the long packet header (Section 17.2) during handshake.
            //   The sequence number of the initial connection ID is 0.
            //   if the preferred_address transport parameter is sent, the sequence number of the supplied
            //   connection ID is 1.
            // - Additional connection IDs are communicated to the peer using NEW_CONNECTION_ID frames (section 19.15)
            //   The sequence number on each newly issued connection ID MUST increase by 1.
            //   The connection ID that a client selects for the first DestinationConnectionID field it sends and
            //   any connection ID provided by a Retry packet are not assigned sequence numbers.
            // - When an endpoint issues a connection ID, it MUST accept packets
            //   that carry this connection ID for the duration of the connection or
            //   until its peer invalidates the connection ID via a RETIRE_CONNECTION_ID frame (section 19.16).
            //   Connection IDs that are issured and not retired are considered active;
            //   any active connection ID is valid for use with the currnt connection at any time,
            //   in any packet type. This includes the connection ID issued by the server
            //   via the preferred_address transport parameter.
            // - An endpoint SHOULD ensure that its peer has a sufficient number of
            //   available and unused connection IDs. Endpoints advertise the number of
            //   active connection IDs they are willing to maintain using the active_connection_id_limit transport
            //   parameter. An endpoint MUST NOT provide more connection ID than the peer's limit.
            //   An endpoint MAY send connection IDs that temporary exceed a peer's limit
            //   if the NEW_CONNECTION_ID frame also requires the retirement of
            //   any excess, by including a sufficiently large value in the RetirePriorTo field
            // - A NEW_CONNECTION_ID frame might cause an endpoint to add some action connection IDs
            //   and retire others based on the value of the RetirePriorTo field.
            //   After processing a NEW_CONNECTION_ID frame and adding and retireing active connection IDs,
            //   if the number of active connection IDs exceeds the value advertised in its
            //   active_connection_id_limit transport parameter, and endpoint MUST close the
            //   connection with CONNECTION_LIMIT_ERROR
            // - An endpoint SHOULD supply a new connection ID when the peer retires a connection ID.
            //   If an endpoint provided fewer connection IDs than the peer's active_connection_id_limit,
            //   it MAY supply a new connection ID when it receives a packet with a previously unused connection ID.
            //   An endpoint MAY limit the total number of connection IDs issued for each connection to avoid the risk
            //   of running out of connection IDs; see Section 10.3.2.
            //   An endpoint MAY also limit the issuance of connection IDs to reduce the
            //   amount of per-path state it maintains, such as path validation status,
            //   as its peer might interact with it over as many paths as there are issued connection IDs.
            // - An endpoint that initiates migration and requires non-zero-length connection IDs
            //   SHOULD ensure that the pool of connection IDs avilable to its peer allows the peer to use a new
            //   connection IDs on migration, as the peer will be unable to respond if the pool is exhausted.
            // - An endpoint that selects a zero-length connection ID during the handshake
            //   cannnot issue a new connection ID. A zero-length DestinationConnectionID field
            //   is used in all packets sent toward such an endpoint over any network path.
            struct tag_section_5_1_1;

            // 5.1.2 Consuming and Retireing Connection IDs
            // - An endpoint can change the connection ID it uses for a peer to anohter available one at any time
            //   during the conneciton. An endpoint consumes connection IDs in response to a migrating peer;see Section 9.5
            // - An endpoint maintains a set of connection IDs received from its peer, any of which it
            //   can use when sending packets.
            //   When the endpoint wishes to remove a connection ID from use, it sends a RETIRE_CONNECTION_ID frame
            //   to its peer. Sending a RETIRE_CONNECTION_ID frame indicates that the connection ID will not be used
            //   again and requests that the peer replace it with a new connection ID using a NEW_CONNECTION_ID frame.
            // - As discussed in Section 9.5, endpoints limit the use of a connection ID to packets sent
            //   from a single local address to a single destination address.
            //   Endpoints SHOULD retire connection IDs when they are no longer actively using either the
            //   local or distination address for which the connection ID was used.
            // - An endpoint might need to stop accepting previously issued connection IDs in certain circumstance.
            //   Such an endpoint can cause its peer to retire connection IDs by sending a NEW_CONNECTION_ID frame
            //   with an increased RetirePriorTo field. The endpoint SHOULD continue to accept
            //   the previously issued connection IDs until they are retired by the peer.
            //   If the endpoint can no longer process the indicated connection IDs, it MAY close the connection.
            // - Upon recipt of an increased RetirePriorTo field, the peer MUST stop using the corresponding
            //   connection IDs and retire them with RETIRE_CONNECTION_ID frames before adding the newly provided
            //   connection ID to the set of active connection IDs. This ordering allows an endpoint
            //   to replace all active connection IDs without the possiblity of a peer having no available connection IDs
            //   and exceeding the limit the peer sets in the active_connection_id_limit transport parameter;see Section 18.9
            //   Failure to cause using the connection IDs when requrested can result in connection failures,
            //   as the issuing endpoint might be unable to continue using the connection IDs with the active connection.
            // - An endpoint SHOULD limit the number of connection IDs it has retired locally
            //   for which RETIRE_CONNECTION_ID frames have not yet been acknowledged.
            //   An endpoint SHOULD allow for sending and tracking a number of RETIRE_CONNECTION_ID frames of
            //   at least twice the value of active_connection_id_limit transport parameter,
            //   An endpoint MUST NOT forget a connection ID without retireing it, though it MAY choose to
            //   treat having connection IDs in need of retirement that exceed this limit as a
            //   CONNECTION_ID_LIMIT_ERROR
            // - Endpoints SHOULD NOT issue updates of the RetireProirTo field before receiving RETIRE_CONNECTION_ID frames
            //   that retire all connection IDs indicated by the previous RetireProirTo value.
            //   (訳:前回(エンドポイントが送信した)RetireProirToの値によって示されたすべてのコネクションIDを引退させる
            //       RETIRE_CONNECTION_IDフレームを受け取るまで,エンドポイントは
            //       RetireProirToの更新を発行してはなりません)
            struct tag_section_5_1_2;

            // 5.2 Matching Packets to Connections
            // - Incoming packets are classfied on recipt.
            //   Packets can either be associated with an existing connection or -- for servers --
            //   potentially create a new connection.
            // - Endpoints try to associate a packet with an existing connection.
            //   If the packet has a non-zero-length Destination Connection ID corresponding to
            //   an existing connection, QUIC processes that packet accordingly.
            //   Note that more than one connection ID can be associated with a connection;see Section 5.1
            // - If the DestinationConnectionID is zero length and the addressing information
            //   in the packet matches the addressing information the endpoint uses to identify a connection
            //   with a zero-length connection ID, QUIC processes the packet as part of that connection.
            //   And endpoint can use just destination IP and port or both source and destination addresses for
            //   identification, though this makes connections fragile as described in Section 5.1.
            // - Endpoints can send a StatelessReset (Section 10.3) for any packets
            //   that cannot be attibuted to an exsting connection.
            //   A StatelessReset allows a peer to more quickly identify when a connection becomes unusable.
            // - Packets that are matched to an exsting connection are discarded if the packets are
            //   inconsistent with the state of that connection
            //   For example, packets are discarded if they indicate a different protocol version than that
            //   of the connection or if the removal of packet protection is unsuccessful once
            //   the expected keys are avilable.
            // - Invalid packets that lack strong integrity protection, such as Initial, Retry,or VersionNegotiation packets,
            //   MAY be discarded. An endpoint MUST generate a connection error if processing the contents of these
            //   packets prior to discovering an error, or fully revert any changes made during that processing.
            struct tag_section_5_2;

            // 5.2.1 Client Packet Handling
            // - Valid packets sent to clients always include a DestinationConnectionID
            //   that matches a value the client selects.
            //   Clients that choose to receive zero-length connection IDs can use
            //   the local address and port to identify a connection.
            //   Packets that do not match an existing connection
            //   -- based on Destination Connection ID or, if this value is zero length,
            //   local IP address and port -- are discarded.
            // - Due to packet recording or loss, a client might receive packets for a
            //   connection that are encrypted with a key it has not yet computed.
            //   The client MAY drop these packets, or it MAY buffer them in anticipation of later
            //   packets taht allow it to compute the key.
            // - If a client receives a packet that uses a different version than it initially
            //   selected, it MUST discard that packet.
            struct tag_section_5_2_1;

            // 5.2.2 Server Packet Handling
            // - If server receives a packet that indicates an unsupported version and
            //   if the packet is large enough to initiate a new connection for any supported version,
            //   the server SHOULD send a VersionNegotiation packet as described in Section 6.1.
            //   A server MAY limit the number of packets to which it responds with a VersionNegotiation packet.
            //   Servers MUST drop smaller packets that specify unsupported versions.
            // - The first packet for an unsupported version can use different semantics and
            //   encodings for any version-specific field. In particular,
            //   different packet protection keys might be used for different versions.
            //   Servers that do not support a particular version are unlikely to be able to decrypt the payload
            //   of the packet or properly interpret the result.
            //   Servers SHOULD respond with a Version Negotiation packet, provided that the datagram is sufficientry long.
            // - Packets with a supported version, or no Version field, are matched to a connection using
            //   the connection ID or -- for packets with zero-length connection IDs -- the local address and port.
            //   These packets are processed using selected connectio; otherwise, the server continues as described
            //   below.
            // - If the packet is an Initial packet fully conforming with the speciification, the server
            //   proceeds with the handshake (Section 7). This commits the server to the version that the client selected.
            //   If a server refuses to accept a new connection, it SHOULD send an Initial packet containing a CONNECTION_CLOSE frame
            //   with error code CONNECTION_REFUSED
            // - If the packet is a 0-RTT packet, the server MAY buffer a limited number of these packets in anticipation of a late-arriving
            //   Initial packet. Clients are no able to send Handshake packets prior to receiving a server response,
            //   so servers SHOULD any such packets.
            // - Servers MUST drop incoming packets under all other circumstances.
            struct tag_section_5_2_2;

            // 5.2.3 Considerations for Simple Load Balancer
            // - A server could load-balance among servers using only source and destination IP addresses and ports.
            //   Changes to the client's IP address or port could result in packets being forwarded to the wrong server.
            //   Such a server deployment could use one of the following methods for connection continuty when a client's address chages.
            // * Servers could use an out-of-band mechanism to forward packets to the correct server based on connection ID
            // * If servers can use ad dedicated server IP address or port, other than the one that the client
            //   initially connect to, they could use the preferred_address transport parameter to request that clients move
            //   connections to that dedicated address. Not that clients could choose not to use the preferred address.
            // - A server in deployment that does not implement a solution to maintain connection
            //   continuty when the client address changes SHOULD indicate that migration is not supported by using
            //   ths disable_active_migration transport parameter. The disable_active_transport_parameter
            //   does not prohibit connection migration after a client has acted on a preferred_address transport parameter.
            // - Sever deployments that use this simple form of load balancing MUST avoid
            //   ceration of a stateless reset oracle;See section 21.11
            struct tag_section_5_2_3;

            // 5.3 Operation on Connections
            // - QUIC doesn't define API but operation set that implementation should provide
            // - client can do
            // * open a connection, which begins the exchange described in Section 7.
            // * enable EarlyData when available
            // * be informed when EarlyData has been accepted or rejected by server
            // - server can do
            // * listen for incoming connections;which prepares for the exchange described in section 7
            // * if EarlyData is supported, embed application data in the TLS resumpion
            //   ticket sent to the client
            // * if EarlyData is supported, retrieve application-controlled data from
            //   client's resumpion ticket and accept or reject EarlyData based on that information
            // - both can do
            // * configure minimum values for the initial number of permitted streams of
            //   each type, as communicated in the transport parameters (Section 7.4)
            // * control resource allocation for receive buffers by setting flow control limits
            //   both for streams and connection
            // * identify whether the handshake has completed successfully or is still ongoing.
            // * keep a connection from silently closing, by either generating PING frames (Section 19.2)
            //   or requesting that the transport send additional frames before the idle timeout expires(Section 10.1)
            // * immediately close (section 10.2) the connection.
            struct tag_section_5_3;
        }  // namespace conn

        using byte = std::uint8_t;
        using ushort = std::uint16_t;
        using uint = std::uint32_t;
        using tsize = std::uint64_t;

        namespace config {
            struct TransportParameter {
                tsize max_streams;
                tsize first_stream_id_of_type;
                tsize preferred_address;
                tsize active_connection_id_limit;
                tsize disable_active_migration;
                bool can_open(tsize id) const {
                    return id < max_streams * 4 + first_stream_id_of_type;
                }

                tsize tracking_RETIRE_CONENCTIN_ID_count_least() const {
                    return active_connection_id_limit * 2;
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

                struct NewConnectionID {
                    int RetirePriorTo;
                };

            }  // namespace impl
        }      // namespace frame

        namespace packet {
            // section 17.2
            /*
            struct LongHeader {
                byte* SrcConnectionID;
                byte* DstConnectionID;
            };

            struct ShortHeader {
                byte* DstConnectionID;
            };
            */
            
            enum class types {
                Initial = 0x0,
                ZeroRTT = 0x1,  // 0-RTT
                HandShake = 0x2,
                Retry = 0x3,
                VersionNegotiation,  // section 17.2.1

                OneRTT,  // 1-RTT

            };

            using number = tsize;

            // 6.Version Negotiation
            // - Version negotiation allows a sever to indicate that it does not support the
            //   verstion the client used. A server sends a VersionNegotiation packet in response
            //   to each packet that might initiate a new connection; see Section 5.2
            // - The size of the first packet sent by client will detemine whether a server
            //   sends a VersionNegotiation packet. Clients that support multiple QUIC version
            //   SHOULD ensure that the first UDP datagram they send is sized to the least of
            //   the minimum datagram sizes from all versions they support,
            //   using PADDING frames (section 19.1) as necessary.
            //   This ensure that the server resoibds if there is a mutually supported version.
            // - A server might not send a VersionNegotiation packet if the datagram it receives
            //   is smaller than the minimum size sepecified in a different version; see Section 14.1.
            struct tag_section_6_0;

            // 6.1. Sending Version Negotiation Packets
            // - If the version selected by the client is no acceptable to the server,
            //   the server responds with a VersionNegotiation packet; see Section 17.2.1
            //   This includes a list of versions that the server will accept.
            //   An endpoint MUST NOT send a VersionNegotiation packet in response to receving
            //   a VersionNegotiation packet
            // - This system allows  a server to process packets with unsupported versions
            //   without retaining state. Though either the Initial packet or the
            //   VersionNegotiation packet that is sent in response could be lost,
            //   the client will send new packets until it sucessfully receives a response
            //   or it abandons the connection attemnpt
            // - A server MAY limit the number of VersionNegotiation packets it sends.
            //   For instance, a server that is able to recognize packets as 0-RTT might
            //   choose not to send VersionNegotiation packets in response to 0-RTT packets
            //   with the expectation that it will eventually receive an Initial pakcet.
            struct tag_section_6_1;

            // 6.2. Handling Version Negotiation Packets
            // - VersionNegotiation packet are designed to allow for functionality to be
            //   defined in the future that allows QUIC to negotiate the version of QUIC to
            //   use for a connection. Future Standards Trace specifications might change
            //   how implementations the support multiple versions of QUIC react to
            //   VersionNegotiation packets received in response to an attempt to establish
            //   a connection using this version.
            // - A client that supports only Version.1 of QUIC MUST abandaon the current connection
            //   attemnpt if it recevies a VersionNegotiation packet, with  the following two exceptions.
            //   A client MUST discard any VersionNegotiation packet if it has
            //   recived and successfully processed any other packet, including an earlier
            //   VersionNegotiation packet. A client MUST discard a VersionNegotiation packet
            //   that lists the QUIC version selected by the client
            // - How to perform version negotiation is left as future work defined by future
            //   Standards Track specfications. In particular that futue work will ensure
            //   robustness against version downgrade attacks; see Section 21.12
            struct tag_section_6_2;

        }  // namespace packet

        using Direction = bool;
        using Mode = bool;
        constexpr Mode client = true;
        constexpr Mode server = false;

        constexpr Direction client_to_server = true;
        constexpr Direction server_to_client = false;

        namespace crypto {

            // 7. Cryptographic and Transport Handshake
            // - QUIC relies on a combined cryptographic and transport handshake to
            //   minimize connection establishment latency. QUIC uses the CRYPTO
            //   frame (section 19.6) to transmit the cryptographic handshake.
            //   The version of QUIC defined in this document is identified
            //   0x00000001 and uses TLS as described in [QUIC-TLS];
            //   a different QUIC version could indicate that a different cryptographic
            //   handshake protocol is in use
            // - QUIC provides reliable, ordered delivery of the cryptographic handsheke
            //   data. QUIC packet protectiin is used to encrypt as much of the handshake
            //   protocol as possible. The cryptographic handshake MUST provide
            //   the following properties
            // * authenticated key exchange where
            //   + a server is always authenticated
            //   + a client is optionally authenticated
            //   + every connection produces distinct and unrelated keys, and
            //   + keying material is usable for packet protection both 0-RTT and 1-RTT
            //     packets
            // * authenticated exchange of values for transport parameters of both endpoints,
            //   and confidentiality protection for server transport port parameter (section 7.4)
            // * authenticated negotiation of an application protocol
            //   (TLS uses ALPN for this purpose)
            // - The CRYPTO frame can be sent in defferent packet number spaces (section 12.3)
            //   The offsets used by CRYPTO frames to ensure ordered delivery of cryptographic handshake
            //   data start from zero in each packet number space.
            // - Figure 4 shows a simplified handshake and the exchange of packets
            //   and frames that are used to advance the handshake.
            //   Exchange of application data during the handshake is enabled where possible.
            //   Once the handshakes complete, enddpoints are able to exchange application
            //   data freely
            // - Endpoints can use packets sent during the handshake to sent to test for
            //   Explicit Congestion Notification (ECN)(訳:明示的輻輳通知)
            //   support; see Section 13.4.
            //   An endpoint validates support for ECN by observing whether the ACK frames
            //   acknowledging the first carry ECN counts, as described in section 13.4.2
            // - Endpoints MUST explicitly negotiate an application protocol. This avoids
            //   situations where there is a disagreement about the protocol that is in use.
            struct tag_section_7_0;

            struct FlowOfHandshake {
                Direction direction;
                packet::types packet;
                frame::types frame;
            };

            // Figure 4 Simplified QUIC Handshake
            constexpr FlowOfHandshake handshake_flow[] = {
                {client_to_server, packet::types::Initial, frame::types::CRYPTO},
                {client_to_server, packet::types::ZeroRTT /*, Application Data*/},
                {server_to_client, packet::types::Initial, frame::types::CRYPTO},
                {server_to_client, packet::types::HandShake, frame::types::CRYPTO},
                {server_to_client, packet::types::OneRTT, /*, Application Data*/},
                {client_to_server, packet::types::HandShake, frame::types::CRYPTO},
                {client_to_server, packet::types::OneRTT, /*, Application Data*/},
                {server_to_client, packet::types::OneRTT, frame::types::HANDSHAKE_DONE},
                // start communication...
            };

            constexpr auto QUIC_version_1 = 0x00000001;

        }  // namespace crypto
    }      // namespace quic
}  // namespace utils

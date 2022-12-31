/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/frame_handler.h>
#include <dnet/httpstring.h>

namespace utils {
    namespace dnet {
        namespace quic::handler {
            error::Error on_ack(QUICContexts* q, frame::ACKFrame<easy::Vec>& ack, ack::PacketNumberSpace space) {
                if (auto err = q->ackh.on_ack_received(ack, space)) {
                    return err;
                }
                return error::none;
            }

            error::Error handle_peer_transport_parameter(QUICContexts* q) {
                size_t len = 0;
                auto data = q->crypto.tls.get_peer_quic_transport_params(&len);
                if (!data) {
                    // handshake not completed?
                    return error::none;
                }
                trsparam::TransportParameter param;
                ByteLen b{(byte*)data, len};
                while (b.len) {
                    if (!param.parse(b)) {
                        return QUICError{
                            .msg = "failed to parse transport error",
                            .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                            .frame_type = FrameType::CRYPTO,
                        };
                    }
                    if (auto err = q->params.set_peer(param, q->flags.is_server)) {
                        return err;
                    }
                }
                q->crypto.got_peer_transport_param = true;
                return error::none;
            }

            error::Error on_crypto(QUICContexts* q, frame::CryptoFrame& c, crypto::EncryptionLevel level) {
                auto& set = q->crypto.crypto_data[int(level)];
                bool added = false;
                auto offset = c.offset.value;
                // check offset
                // avoid heap allocation
                if (set.read_offset == offset) {
                    if (auto err = q->crypto.tls.provide_quic_data(level, c.crypto_data.data(), c.crypto_data.size())) {
                        return err;
                    }
                    set.read_offset += c.crypto_data.size();
                    added = true;
                }
                else {
                    crypto::CryptoData store;
                    store.offset = offset;
                    store.b = ConstByteLen{c.crypto_data.data(), c.crypto_data.size()};
                    if (!store.b.valid()) {
                        return error::memory_exhausted;
                    }
                    set.data.push_back(std::move(store));
                    auto cmp = [](crypto::CryptoData& a, crypto::CryptoData& b) {
                        return a.offset < b.offset;
                    };
                    if (set.data.size() > 1) {
                        std::sort(set.data.begin(), set.data.end(), cmp);
                    }
                }
                while (set.data.size() &&
                       set.read_offset == set.data[0].offset) {
                    auto dat = set.data[0].b.unbox();
                    q->crypto.tls.provide_quic_data(level, dat.data, dat.len);
                    set.read_offset += dat.len;
                    added = true;
                    set.data.pop_front();
                }
                if (added) {
                    error::Error err;
                    if (q->crypto.established) {
                        err = q->crypto.tls.progress_quic();
                    }
                    else {
                        if (q->flags.is_server) {
                            err = q->crypto.tls.accept();
                        }
                        else {
                            err = q->crypto.tls.connect();
                        }
                    }
                    if (err) {
                        return handle_crypto_error(q, std::move(err));
                    }
                    q->crypto.established = true;
                    if (!q->crypto.got_peer_transport_param) {
                        return handle_peer_transport_parameter(q);
                    }
                    return error::none;
                }
                return error::none;
            }

            struct ConnectionCloseError {
                TransportError err;
                dnet::String msg;
                FrameType type;

                void error(auto&& pb) {
                    QUICError{
                        .msg = msg.c_str(),
                        .transport_error = err,
                        .frame_type = type,
                    }
                        .error(pb);
                }
            };

            error::Error on_connection_close(QUICContexts* q, frame::ConnectionCloseFrame& c) {
                q->errs.close_err = ConnectionCloseError{
                    .err = TransportError(c.error_code.value),
                    .msg = String((const char*)c.reason_phrase.data(), c.reason_phrase.size()),
                    .type = FrameType(c.frame_type.value),
                };
                q->state = QState::closed;
                return error::none;
            }

            error::Error on_new_connection_id(QUICContexts* q, frame::NewConnectionIDFrame& c) {
                if (q->flags.zero_connID) {
                    return QUICError{
                        .msg = "zero-length connection ID is used but NEW_CONNECTION_ID received",
                        .rfc_ref = "rfc9000 19.15 NEW_CONNECTION_ID Frames",
                        .rfc_comment = "An endpoint that is sending packets with a zero-length Destination Connection ID MUST treat receipt of a NEW_CONNECTION_ID frame as a connection error of type PROTOCOL_VIOLATION.",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .packet_type = PacketType::OneRTT,
                        .frame_type = FrameType::NEW_CONNECTION_ID,
                    };
                }
                if (c.length < 1 || 20 < c.length) {
                    return QUICError{
                        .msg = "invalid connection ID length for QUIC version 1",
                        .rfc_ref = "rfc9000 19.15 NEW_CONNECTION_ID Frames",
                        .rfc_comment = "Values less than 1 and greater than 20 are invalid and MUST be treated as a connection error of type FRAME_ENCODING_ERROR.",
                        .transport_error = TransportError::FRAME_ENCODING_ERROR,
                        .packet_type = PacketType::OneRTT,
                        .frame_type = FrameType::NEW_CONNECTION_ID,
                    };
                }
                if (c.retire_proior_to > c.sequecne_number) {
                    return QUICError{
                        .msg = "retire_proir_to field is grater than sequence_number",
                        .rfc_ref = "rfc9000 19.15 NEW_CONNECTION_ID Frames",
                        .rfc_comment = "Receiving a value in the Retire Prior To field that is greater than that in the Sequence Number field MUST be treated as a connection error of type FRAME_ENCODING_ERROR.",
                        .transport_error = TransportError::FRAME_ENCODING_ERROR,
                        .packet_type = PacketType::OneRTT,
                        .frame_type = FrameType::NEW_CONNECTION_ID,
                    };
                }
                if (q->dstIDs.retire(c.retire_proior_to)) {
                    if (auto err = q->frames.push(ack::PacketNumberSpace::application,
                                                  frame::RetireConnectionIDFrame{.sequence_number = c.retire_proior_to})) {
                        return err;
                    }
                }
                if (!q->dstIDs.add_new(c.sequecne_number, {c.connectionID.data(), c.connectionID.size()}, c.stateless_reset_token)) {
                    return QUICError{
                        .msg = "failed to add remote connectionID",
                    };
                }
                return error::none;
            }

            error::Error on_streams_blocked(QUICContexts* q, frame::StreamsBlockedFrame& blocked) {
                return error::none;
            }

            error::Error on_handshake_done(QUICContexts* q) {
                q->state = QState::established;
                return error::none;
            }
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils

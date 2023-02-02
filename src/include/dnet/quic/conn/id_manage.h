/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// id_manage - id management
#pragma once
#include "connection_id.h"
#include "../frame/conn_manage.h"
#include "../transport_error.h"
#include "../ack/ack_lost_record.h"
#include "../stream/fragment.h"
#include "../ioresult.h"
#include "../frame/writer.h"

namespace utils {
    namespace dnet::quic::conn {

        struct IDManager {
            IDAcceptor acceptor;
            IDIssuer issuer;
            storage initial_random;

            view::rvec gen_initial_random(byte len = 8) {
                if (len < 8) {
                    len = 8;
                }
                initial_random = make_storage(len);
                if (initial_random.null()) {
                    return {};
                }
                if (!issuer.gen_random(initial_random)) {
                    return {};
                }
                return initial_random;
            }

            error::Error recv_new_connection_id(const frame::NewConnectionIDFrame& c) {
                if (acceptor.use_zero_length) {
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
                if (c.retire_proior_to > c.sequence_number) {
                    return QUICError{
                        .msg = "retire_proir_to field is grater than sequence_number",
                        .rfc_ref = "rfc9000 19.15 NEW_CONNECTION_ID Frames",
                        .rfc_comment = "Receiving a value in the Retire Prior To field that is greater than that in the Sequence Number field MUST be treated as a connection error of type FRAME_ENCODING_ERROR.",
                        .transport_error = TransportError::FRAME_ENCODING_ERROR,
                        .packet_type = PacketType::OneRTT,
                        .frame_type = FrameType::NEW_CONNECTION_ID,
                    };
                }
                acceptor.retire(c.retire_proior_to);
                if (!acceptor.accept(c.sequence_number, c.connectionID, c.stateless_reset_token)) {
                    return QUICError{
                        .msg = "failed to add remote connectionID",
                    };
                }
                return error::none;
            }

            void recv_retire_connection_id(const frame::RetireConnectionIDFrame& c) {
                issuer.retire(c);
            }

            IOResult send_new_connection_id(auto&& observer_vec, frame::fwriter& w) {
                auto do_render = [&](auto& f) -> IOResult {
                    frame::NewConnectionIDFrame frame;
                    frame.sequence_number = f.second.id.seq;
                    frame.connectionID = f.second.id.id;
                    view::copy(frame.stateless_reset_token, f.second.id.stateless_reset_token);
                    frame.retire_proior_to = f.second.retire_proior_to;
                    if (w.remain().size() < frame.len()) {
                        return IOResult::no_capacity;
                    }
                    return w.write(frame) ? IOResult::ok : IOResult::fatal;
                };
                for (auto& f : issuer.idlist) {
                    if (!f.second.needless_ack) {
                        if (!f.second.wait || f.second.wait->is_lost()) {
                            if (auto res = do_render(f); res == IOResult::fatal) {
                                return IOResult::fatal;
                            }
                            else if (res == IOResult::ok) {
                                if (f.second.wait) {
                                    f.second.wait->wait();
                                }
                                else {
                                    f.second.wait = ack::make_ack_wait();
                                }
                                observer_vec.push_back(f.second.wait);
                            }
                        }
                        else if (f.second.wait->is_ack()) {
                            f.second.wait = nullptr;
                            f.second.needless_ack = true;
                        }
                    }
                }
                return IOResult::ok;
            }

            IOResult send_retire_connection_id(auto&& observer_vec, frame::fwriter& w) {
                for (auto it = acceptor.retire_wait.begin(); it != acceptor.retire_wait.end();) {
                    if (!it->second.wait || it->second.wait->is_lost()) {
                        frame::RetireConnectionIDFrame retire;
                        retire.sequence_number = it->second.seq;
                        if (w.remain().size() < retire.len()) {
                            continue;
                        }
                        if (!w.write(retire)) {
                            return IOResult::fatal;
                        }
                        if (it->second.wait) {
                            it->second.wait->wait();
                        }
                        else {
                            it->second.wait = ack::make_ack_wait();
                        }
                        observer_vec.push_back(it->second.wait);
                    }
                    else if (it->second.wait->is_ack()) {
                        it = acceptor.retire_wait.erase(it);
                        continue;
                    }
                    it++;
                }
                return IOResult::ok;
            }

            auto get_dstID_len_callback() {
                return [this](io::reader r, size_t* len) {
                    for (auto& id : issuer.idlist) {
                        auto& target = id.second.id.id;
                        auto cmp = r.remain().substr(0, target.size());
                        if (cmp == target) {
                            *len = target.size();
                            return true;
                        }
                    }
                    return false;
                };
            }

            bool accept_initial(view::rvec id) {
                if (acceptor.max_seq != -1) {
                    return false;
                }
                byte stateless_reset_token[16]{};
                return acceptor.accept(0, id, stateless_reset_token);
            }

            void add_initial_stateless_reset_token(byte (&stateless_reset_token)[16]) {
                if (auto found = acceptor.idlist.find(0); found != acceptor.idlist.end()) {
                    view::copy(found->second.stateless_reset_token, stateless_reset_token);
                }
            }

            void accept_transport_param(view::rvec id, byte (&stateless_reset_token)[16]) {
                acceptor.accept(1, id, stateless_reset_token);
            }
        };
    }  // namespace dnet::quic::conn
}  // namespace utils

/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "external/id_exporter.h"
#include "common_param.h"
#include "../ack/ack_lost_record.h"
#include "../frame/writer.h"
#include "../frame/conn_id.h"
#include "../transport_error.h"
#include "../time.h"
#include "../resend/ack_handler.h"

namespace futils {
    namespace fnet::quic::connid {
        struct IDWait {
            std::int64_t seq = -1;
            std::int64_t retire_proior_to = 0;
            resend::ACKHandler wait;
        };

        constexpr auto default_max_active_conn_id = 2;

        template <class TConfig>
        struct IDIssuer {
           private:
            template <class K, class V>
            using ConnIDMap = typename TConfig::template connid_map<K, V>;
            template <class V>
            using WaitQue = typename TConfig::template wait_que<V>;
            std::int64_t issued_seq = invalid_seq;
            SequenceNumber current_seq = invalid_seq;
            ConnIDMap<std::int64_t, IDStorage> srcids;
            WaitQue<IDWait> waitlist;
            std::uint64_t max_active_conn_id = default_max_active_conn_id;
            byte connID_len = 0;
            byte concurrent_limit = 0;
            ConnIDExporter exporter;
            std::int64_t retire_proior_to_id = 0;

           public:
            void reset(const ConnIDExporter& exp, byte conn_id_len, byte conc_limit) {
                srcids.clear();
                waitlist.clear();
                max_active_conn_id = default_max_active_conn_id;
                issued_seq = invalid_seq;
                current_seq = invalid_seq;
                connID_len = conn_id_len;
                concurrent_limit = conc_limit;
                exporter = exp;
            }

            // expose_close_ids expose connIDs to close
            // after call this, IDIssuer is invalid
            void expose_close_ids(auto&& ids) {
                for (auto&& s : srcids) {
                    IDStorage& exp = s.second;
                    CloseID c{.id = std::move(exp.id)};
                    view::copy(c.token, exp.stateless_reset_token);
                    ids.push_back(std::move(c));
                }
            }

            void on_transport_parameter_received(std::uint64_t active_conn_id) {
                max_active_conn_id = active_conn_id;
            }

            constexpr bool is_using_zero_length() const {
                return connID_len == 0;
            }

            constexpr byte get_connID_len() const {
                return connID_len;
            }

            std::pair<ConnID, error::Error> issue(CommonParam& cparam, bool enque_wait) {
                if (connID_len == 0) {
                    return {{}, error::Error("using zero length", error::Category::lib, error::fnet_quic_connection_id_error)};
                }
                if (!cparam.random.valid()) {
                    return {{}, error::Error("invalid random", error::Category::lib, error::fnet_quic_user_arg_error)};
                }
                std::int64_t retire_proior_to = 0;
                if (srcids.size() >= max_active_conn_id) {
                    if (retire_proior_to_id != current_seq) {
                        return {{}, error::Error("id generation limit", error::Category::lib, error::fnet_quic_connection_id_error)};
                    }
                    retire_proior_to_id = current_seq + 1;
                    retire_proior_to = current_seq;
                }
                IDStorage id;
                id.id = make_storage(connID_len);
                auto err = exporter.generate(cparam.random, cparam.version, connID_len, id.id, id.stateless_reset_token);
                if (err) {
                    return {{}, err};
                }
                id.seq = issued_seq + 1;
                IDStorage& new_issued = srcids[id.seq];
                if (new_issued.seq.valid()) {
                    // library bug!!
                    return {{}, error::Error("invalid sequence id for new connection id. libary bug!!", error::Category::lib, error::fnet_quic_implementation_bug)};
                }
                issued_seq++;
                if (issued_seq == 0) {
                    // on initial issue, use connection ID with sequence number 0
                    current_seq = 0;
                }
                new_issued = std::move(id);
                if (enque_wait) {
                    waitlist.push_back({new_issued.seq, retire_proior_to});
                }
                exporter.add(new_issued.id, new_issued.stateless_reset_token);
                return {new_issued.to_ConnID(), error::none};
            }

            bool send(frame::fwriter& fw, auto&& observer) {
                auto do_send = [&](IDWait& id_wait, ConnID id) {
                    frame::NewConnectionIDFrame new_conn;
                    new_conn.sequence_number = id.seq.seq;
                    new_conn.connectionID = id.id;
                    view::copy(new_conn.stateless_reset_token, id.stateless_reset_token);
                    new_conn.retire_proior_to = id_wait.retire_proior_to;
                    if (fw.remain().size() < new_conn.len()) {
                        return true;  // wait next chance
                    }
                    if (!fw.write(new_conn)) {
                        return false;
                    }
                    id_wait.wait.wait(observer);
                    return true;
                };
                for (auto it = waitlist.begin(); it != waitlist.end();) {
                    if (it->wait.not_confirmed()) {
                        if (it->wait.is_ack()) {
                            it->wait.confirm();
                            it = waitlist.erase(it);
                            continue;
                        }
                        else if (it->wait.is_lost()) {
                            auto id = choose(it->seq);
                            if (!id.seq.valid()) {
                                it = waitlist.erase(it);  // already retired
                                continue;
                            }
                            if (!do_send(*it, id)) {
                                return false;
                            }
                        }
                    }
                    else {
                        auto id = choose(it->seq);
                        if (!id.seq.valid()) {
                            it = waitlist.erase(it);  // already retired
                            continue;
                        }
                        if (!do_send(*it, id)) {
                            return false;
                        }
                    }
                    it++;
                }
                return true;
            }

           private:
            error::Error update_current_id() {
                current_seq = invalid_seq;
                for (auto& s : srcids) {
                    if (!current_seq.valid() || s.second.seq < current_seq) {
                        current_seq = s.second.seq;
                    }
                }
                if (!current_seq.valid()) {
                    return QUICError{
                        .msg = "retire connection ID without new connection ID. libary bug!",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }
                return error::none;
            }

           public:
            error::Error retire(CommonParam& cparam, view::rvec recv_dest_id, const frame::RetireConnectionIDFrame& retire) {
                if (is_using_zero_length()) {
                    return QUICError{
                        .msg = "Received RetireConnectionID frame while using zero-length connection ID",
                        .transport_error = TransportError::CONNECTION_ID_LIMIT_ERROR,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }

                if (retire.sequence_number > issued_seq) {
                    return QUICError{
                        .msg = "Retiring connection ID with a sequence number that was not issued",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }

                auto to_retire = choose(retire.sequence_number);
                if (!to_retire.seq.valid()) {
                    return error::none;  // Already retired
                }

                if (to_retire.id == recv_dest_id) {
                    return QUICError{
                        .msg = "Retiring connection ID that was used for sending the RetireConnectionID frame",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }

                exporter.retire(to_retire.id, to_retire.stateless_reset_token);

                if (!srcids.erase(retire.sequence_number)) {
                    return QUICError{.msg = "Failed to delete connection ID. library bug!!"};
                }

                if (to_retire.seq != 0) {
                    if (auto [_, err] = issue(cparam, true); err) {
                        return err;
                    }
                }

                if (current_seq == retire.sequence_number) {
                    return update_current_id();
                }

                return error::none;
            }

            ConnID choose(std::int64_t sequence_number) {
                if (auto found = srcids.find(sequence_number); found != srcids.end()) {
                    return found->second.to_ConnID();
                }
                return {};
            }

            bool has_id(view::rvec cmp) const {
                for (auto& id : srcids) {
                    if (id.second.id == cmp) {
                        return true;
                    }
                }
                return false;
            }

            view::rvec pick_up_id() {
                if (!current_seq.valid()) {
                    return {};
                }
                return choose(current_seq).id;
            }

            auto get_onertt_dstID_len_callback() {
                return [this](binary::reader r, size_t* len) {
                    if (this->is_using_zero_length()) {
                        *len = 0;
                        return true;
                    }
                    for (auto& id : srcids) {
                        auto& target = id.second.id;
                        auto cmp = r.remain().substr(0, target.size());
                        if (cmp == target) {
                            *len = target.size();
                            return true;
                        }
                    }
                    return false;
                };
            }

            error::Error issue_ids_to_max_connid_limit(CommonParam& cparam) {
                auto to_issue = max_active_conn_id < concurrent_limit ? max_active_conn_id : concurrent_limit;
                for (auto i = srcids.size(); i < to_issue; i++) {
                    auto [_, err] = issue(cparam, true);
                    if (err) {
                        return err;
                    }
                }
                return error::none;
            }
        };

    }  // namespace fnet::quic::connid
}  // namespace futils

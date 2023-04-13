/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// connection_id - connection id manager
#pragma once
#include "../../storage.h"
#include "../../std/hash_map.h"
#include "../../std/list.h"
#include "../ack/ack_lost_record.h"
#include "../frame/conn_id.h"
#include "random.h"
#include "../../error.h"
#include "../transport_error.h"
#include "../version.h"
#include "../packet/stateless_reset.h"

namespace utils {
    namespace fnet::quic::connid {
        using StatelessResetToken = byte[16];

        struct ConnID {
            std::int64_t seq = -1;
            view::rvec id;
            view::rvec stateless_reset_token;
        };

        struct IDStorage {
            std::int64_t seq = -1;
            storage id;
            StatelessResetToken stateless_reset_token;

            ConnID to_ConnID() const {
                return ConnID{
                    .seq = seq,
                    .id = id,
                    .stateless_reset_token = stateless_reset_token,
                };
            }
        };

        constexpr byte null_stateless_reset[16]{};

        struct IDWait {
            ConnID id;
            std::int64_t retire_proior_to = 0;
            std::shared_ptr<ack::ACKLostRecord> wait;
        };

        constexpr auto default_max_active_conn_id = 2;

        struct InitialRetry {
           private:
            storage initial_random;
            storage retry_random;

            bool gen(storage& target, byte size, Random& rand) {
                if (!rand.valid()) {
                    return false;
                }
                if (size < 8) {
                    size = 8;
                }
                target = make_storage(size);
                return target.size() == size && rand.gen_random(target);
            }

            bool recv(view::rvec id, storage& target) {
                target = make_storage(id);
                return target.size() == id.size();
            }

           public:
            void reset() {
                initial_random.clear();
                retry_random.clear();
            }

            bool gen_initial(bool is_server, byte size, Random& rand) {
                if (is_server) {
                    return false;
                }
                return gen(initial_random, size, rand);
            }

            bool gen_retry(bool is_server, byte size, Random& rand) {
                if (is_server) {
                    return false;
                }
                return gen(retry_random, size, rand);
            }

            bool recv_initial(bool is_server, view::rvec id) {
                if (!is_server) {
                    return false;
                }
                return recv(id, initial_random);
            }

            bool recv_retry(bool is_server, view::rvec id) {
                if (is_server) {
                    return false;
                }
                return recv(id, retry_random);
            }

            bool has_retry() const {
                return retry_random.size() != 0;
            }

            view::rvec get_initial() const {
                return initial_random;
            }

            view::rvec get_retry() const {
                return retry_random;
            }

            view::rvec get_initial_or_retry() const {
                if (has_retry()) {
                    return retry_random;
                }
                else {
                    return initial_random;
                }
            }
        };

        struct IDIssuer {
           private:
            std::int64_t issued_seq = -1;
            std::int64_t most_min_seq = -1;
            slib::hash_map<std::int64_t, IDStorage> srcids;
            slib::list<IDWait> waitlist;
            bool use_zero_length = false;
            std::uint64_t max_active_conn_id = default_max_active_conn_id;

           public:
            void reset(bool use_zero_length) {
                srcids.clear();
                waitlist.clear();
                max_active_conn_id = default_max_active_conn_id;
                issued_seq = -1;
                most_min_seq = -1;
                use_zero_length = use_zero_length;
            }

            void on_transport_parameter_received(std::uint64_t active_conn_id) {
                max_active_conn_id = active_conn_id;
            }

            constexpr bool is_using_zero_length() const {
                return use_zero_length;
            }

            ConnID issue(size_t len, Random& random, bool enque_wait) {
                if (use_zero_length) {
                    return {};
                }
                if (len == 0 || !random.valid()) {
                    return {};
                }
                IDStorage id;
                id.id = make_storage(len);
                if (id.id.null()) {
                    return {};
                }
                if (!random.gen_random(id.id)) {
                    return {};
                }
                if (!random.gen_random(id.stateless_reset_token)) {
                    return {};
                }
                id.seq = issued_seq + 1;
                auto& new_issued = srcids[id.seq];
                if (new_issued.seq != -1) {
                    return {};  // library bug!!
                }
                issued_seq++;
                if (issued_seq == 0) {
                    most_min_seq = 0;
                }
                new_issued = std::move(id);
                if (enque_wait) {
                    waitlist.push_back({new_issued.to_ConnID()});
                }
                return new_issued.to_ConnID();
            }

            bool send(frame::fwriter& fw, auto&& observer_vec) {
                auto do_send = [&](IDWait& it) {
                    frame::NewConnectionIDFrame new_conn;
                    new_conn.sequence_number = it.id.seq;
                    new_conn.connectionID = it.id.id;
                    view::copy(new_conn.stateless_reset_token, it.id.stateless_reset_token);
                    new_conn.retire_proior_to = it.retire_proior_to;
                    if (fw.remain().size() < new_conn.len()) {
                        return true;  // wait next chance
                    }
                    if (!fw.write(new_conn)) {
                        return false;
                    }
                    it.wait = ack::make_ack_wait();
                    observer_vec.push_back(it.wait);
                    return true;
                };
                for (auto it = waitlist.begin(); it != waitlist.end();) {
                    if (it->wait) {
                        if (it->wait->is_ack()) {
                            ack::put_ack_wait(std::move(it->wait));
                            it = waitlist.erase(it);
                            continue;
                        }
                        else if (it->wait->is_lost()) {
                            it->id = choose(it->id.seq);
                            if (it->id.seq == -1) {
                                it = waitlist.erase(it);  // already retired
                                continue;
                            }
                            if (!do_send(*it)) {
                                return false;
                            }
                        }
                    }
                    else {
                        if (!do_send(*it)) {
                            return false;
                        }
                    }
                    it++;
                }
                return true;
            }

            error::Error retire(view::rvec recv_dest_id, const frame::RetireConnectionIDFrame& retire) {
                if (use_zero_length) {
                    return QUICError{
                        .msg = "recv RetireConnectionID frame when using zero length connection ID",
                        .transport_error = TransportError::CONNECTION_ID_LIMIT_ERROR,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }
                if (retire.sequence_number > issued_seq) {
                    return QUICError{
                        .msg = "retiring connection id with sequence number not issued",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }
                auto to_retire = choose(retire.sequence_number);
                if (to_retire.seq == -1) {
                    return error::none;  // already retired
                }
                if (to_retire.id == recv_dest_id) {
                    return QUICError{
                        .msg = "retireing connection id which was used for sending RetireConnectionID frame",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }
                if (!srcids.erase(retire.sequence_number)) {
                    return QUICError{.msg = "failed to delete connection id. library bug!!"};
                }
                if (most_min_seq == retire.sequence_number) {
                    most_min_seq = -1;
                    for (auto& s : srcids) {
                        if (most_min_seq == -1 || s.second.seq < most_min_seq) {
                            most_min_seq = s.second.seq;
                        }
                    }
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
                if (most_min_seq == -1) {
                    return {};
                }
                return choose(most_min_seq).id;
            }

            auto get_onertt_dstID_len_callback() {
                return [this](io::reader r, size_t* len) {
                    if (use_zero_length) {
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
        };

        struct RetireWait {
            std::int64_t seq = -1;
            std::shared_ptr<ack::ACKLostRecord> wait;
        };

        struct IDAcceptor {
           private:
            bool use_zero_length = false;
            slib::hash_map<std::int64_t, IDStorage> dstids;
            slib::list<RetireWait> waitlist;

            std::int64_t highest_accepted = -1;
            std::int64_t active_connid = -1;
            std::int64_t highest_retired = -1;

            void retire_under(std::int64_t border) {
                if (border <= highest_retired) {
                    return;
                }
                for (auto i = highest_retired + 1; i < border; i++) {
                    retire(i);
                }
                highest_retired = border - 1;
            }

            void update_active() {
                for (std::int64_t i = active_connid + 1; i <= highest_accepted; i++) {
                    if (auto selected = choose(i); selected.seq == i) {
                        active_connid = i;
                        break;
                    }
                }
            }

           public:
            void reset() {
                dstids.clear();
                waitlist.clear();
                highest_accepted = -1;
                active_connid = -1;
                highest_retired = -1;
                use_zero_length = false;
            }

            error::Error recv(Version version, const frame::NewConnectionIDFrame& new_conn) {
                return accept(version, new_conn.sequence_number, new_conn.retire_proior_to, new_conn.connectionID, new_conn.stateless_reset_token);
            }

            void on_zero_length_acception() {
                use_zero_length = true;
            }

            error::Error accept(Version version, std::int64_t sequence_number, std::int64_t retire_proior_to, view::rvec connectionID, const StatelessResetToken& stateless_reset_token) {
                if (use_zero_length) {
                    return QUICError{
                        .msg = "recv NewConnectionID frame when using zero length connection ID",
                        .transport_error = TransportError::CONNECTION_ID_LIMIT_ERROR,
                        .frame_type = FrameType::RETIRE_CONNECTION_ID,
                    };
                }
                if (sequence_number < active_connid || sequence_number < highest_retired) {
                    retire(sequence_number);
                    return error::none;
                }
                if (version == version_1) {
                    if (connectionID.size() < 1 || 20 < connectionID.size()) {
                        return QUICError{
                            .msg = "invalid connection ID length for QUIC version 1",
                            .transport_error = TransportError::FRAME_ENCODING_ERROR,
                            .packet_type = PacketType::OneRTT,
                            .frame_type = FrameType::NEW_CONNECTION_ID,
                        };
                    }
                }
                if (retire_proior_to > sequence_number) {
                    return QUICError{
                        .msg = "retire_proior_to is higher than sequence_number",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::NEW_CONNECTION_ID,
                    };
                }
                retire_under(retire_proior_to);
                auto& id = dstids[sequence_number];
                if (id.seq == -1) {
                    id.seq = sequence_number;
                    id.id = make_storage(connectionID);
                    if (id.id.null()) {
                        return error::memory_exhausted;
                    }
                    view::copy(id.stateless_reset_token, stateless_reset_token);
                }
                else {
                    if (id.id != connectionID) {
                        return QUICError{
                            .msg = "different connection ID on same sequence number",
                            .transport_error = TransportError::PROTOCOL_VIOLATION,
                            .frame_type = FrameType::NEW_CONNECTION_ID,
                        };
                    }
                    if (id.stateless_reset_token != view::rvec(stateless_reset_token)) {
                        return QUICError{
                            .msg = "different stateless reset token on same sequence number",
                            .transport_error = TransportError::PROTOCOL_VIOLATION,
                            .frame_type = FrameType::NEW_CONNECTION_ID,
                        };
                    }
                }
                if (id.seq > highest_accepted) {
                    highest_accepted = id.seq;
                }
                if (active_connid < retire_proior_to) {
                    update_active();
                }
                return error::none;
            }

            ConnID choose(std::int64_t sequence_number) const {
                if (auto found = dstids.find(sequence_number); found != dstids.end()) {
                    return found->second.to_ConnID();
                }
                return {};
            }

           private:
            IDStorage* direct_choose(std::int64_t sequence_number) {
                if (auto found = dstids.find(sequence_number); found != dstids.end()) {
                    return &found->second;
                }
                return nullptr;
            }

           public:
            void on_initial_stateless_reset_token_received(const StatelessResetToken& token) {
                if (auto dir = direct_choose(0)) {
                    view::copy(dir->stateless_reset_token, token);
                }
            }

            error::Error on_preferred_address_received(Version version, view::rvec connectionID, const StatelessResetToken& token) {
                return accept(version, 1, -1, connectionID, token);
            }

            bool initial_conn_id_accepted() const {
                return use_zero_length || active_connid != -1;
            }

            view::rvec pick_up_id(const InitialRetry* iniret) const {
                if (use_zero_length) {
                    return {};
                }
                if (active_connid == -1) {
                    return iniret ? iniret->get_initial_or_retry() : view::rvec{};
                }
                return choose(active_connid).id;
            }

            bool has_id(view::rvec cmp) const {
                for (auto& id : dstids) {
                    if (id.second.id == cmp) {
                        return true;
                    }
                }
                return false;
            }

            bool retire(std::int64_t sequence_number) {
                if (!dstids.erase(sequence_number)) {
                    return false;
                }
                waitlist.push_back({sequence_number});
                if (highest_retired < sequence_number) {
                    highest_retired = sequence_number;
                }
                return true;
            }

            bool send(frame::fwriter& fw, auto&& observer_vec) {
                auto do_send = [&](RetireWait& it) {
                    frame::RetireConnectionIDFrame retire;
                    retire.sequence_number = it.seq;
                    if (fw.remain().size() < retire.len()) {
                        return true;  // wait next chance
                    }
                    if (!fw.write(retire)) {
                        return false;
                    }
                    it.wait = ack::make_ack_wait();
                    observer_vec.push_back(it.wait);
                    return true;
                };
                for (auto it = waitlist.begin(); it != waitlist.end();) {
                    if (it->wait) {
                        if (it->wait->is_ack()) {
                            ack::put_ack_wait(std::move(it->wait));
                            it = waitlist.erase(it);
                            continue;
                        }
                        else if (it->wait->is_lost()) {
                            if (!do_send(*it)) {
                                return false;
                            }
                        }
                    }
                    else {
                        if (!do_send(*it)) {
                            return false;
                        }
                    }
                    it++;
                }
                return true;
            }

            auto stateless_reset_callback() {
                return [&](packet::StatelessReset reset) {
                    if (active_connid < 0) {
                        return false;
                    }
                    auto cur = choose(active_connid);
                    if (cur.stateless_reset_token == null_stateless_reset) {
                        return false;
                    }
                    return reset.stateless_reset_token == cur.stateless_reset_token;
                };
            }

            constexpr bool is_using_zero_length() const {
                return use_zero_length;
            }
        };

    }  // namespace fnet::quic::connid
}  // namespace utils

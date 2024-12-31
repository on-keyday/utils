/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// connection_id - connection id manager
#pragma once
#include "common_param.h"
#include "../ack/ack_lost_record.h"
#include "config.h"
#include "../transport_error.h"
#include "../frame/writer.h"
#include "../frame/conn_id.h"
#include "../packet/stateless_reset.h"
#include "../resend/ack_handler.h"

namespace futils {
    namespace fnet::quic::connid {

        struct RetireWait {
            SequenceNumber seq = invalid_seq;
            resend::ACKHandler wait;
        };

        template <class TConfig>
        struct IDAcceptor {
           private:
            template <class K, class V>
            using ConnIDMap = typename TConfig::template connid_map<K, V>;
            template <class V>
            using WaitQue = typename TConfig::template wait_que<V>;

            ConnIDChangeMode change_mode = ConnIDChangeMode::none;
            bool use_zero_length = false;
            std::uint32_t max_packet_per_id = 0;
            ConnIDMap<std::int64_t, IDStorage> dstids;
            WaitQue<RetireWait> waitlist;

            std::int64_t highest_accepted = invalid_seq;
            std::int64_t highest_retired = invalid_seq;
            SequenceNumber active_connid = invalid_seq;

            std::uint32_t packet_count_since_id_changed = 0;
            std::uint32_t packet_per_id = 0;

            void retire_under(std::int64_t border) {
                if (border <= highest_retired) {
                    return;
                }
                for (auto i = highest_retired + 1; i < border; i++) {
                    retire(i);
                }
                highest_retired = border - 1;
            }

            error::Error update_active(CommonParam& cparam) {
                if (dstids.size() < 2 && active_connid.valid()) {
                    return error::none;
                }
                if (active_connid.valid()) {
                    if (!retire(active_connid)) {
                        return QUICError{
                            .msg = "failed to retire active connection id. unexpected error.",
                        };
                    }
                }
                for (std::int64_t i = active_connid + 1; i <= highest_accepted; i++) {
                    if (auto selected = choose(i); selected.seq == i) {
                        active_connid = i;
                        packet_count_since_id_changed = 0;
                        if (change_mode == ConnIDChangeMode::random) {
                            byte d[4]{};
                            cparam.random.gen_random(d, RandomUsage::id_change_duration);
                            binary::reader r{d};
                            std::uint32_t v = 0;
                            binary::read_num(r, v);
                            packet_per_id = v % max_packet_per_id;
                        }
                        return error::none;
                    }
                }
                return QUICError{
                    .msg = "no connection id available",
                };
            }

           public:
            void reset(std::uint32_t p_per_id, std::uint32_t max_p_per_id, ConnIDChangeMode mode) {
                dstids.clear();
                waitlist.clear();
                highest_accepted = invalid_seq;
                active_connid = invalid_seq;
                highest_retired = invalid_seq;
                use_zero_length = false;
                packet_count_since_id_changed = 0;
                packet_per_id = p_per_id;
                if (change_mode == ConnIDChangeMode::random && max_p_per_id == 0) {
                    max_p_per_id = 10000;
                }
                max_packet_per_id = max_p_per_id;
                change_mode = mode;
            }

            error::Error recv(CommonParam& cparam, const frame::NewConnectionIDFrame& new_conn) {
                return accept(cparam, new_conn.sequence_number, new_conn.retire_proior_to, new_conn.connectionID, new_conn.stateless_reset_token);
            }

            void on_zero_length_acception() {
                use_zero_length = true;
            }

            // retire_proior_to means
            error::Error accept(CommonParam& cparam, std::int64_t sequence_number, std::int64_t retire_proior_to, view::rvec connectionID, const StatelessResetToken& stateless_reset_token) {
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
                if (cparam.version == version_1) {
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
                    if (auto err = update_active(cparam)) {
                        return err;
                    }
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
            error::Error maybe_update_id(CommonParam& cparam, bool handshake_complete, std::uint64_t local_max_active_conn) {
                if (!handshake_complete) {
                    return error::none;
                }
                if (dstids.size() && active_connid == 0 ||
                    (change_mode != ConnIDChangeMode::none &&
                     dstids.size() << 1 >= local_max_active_conn &&
                     packet_count_since_id_changed >= packet_per_id)) {
                    return update_active(cparam);
                }
                return error::none;
            }

            void on_initial_stateless_reset_token_received(const StatelessResetToken& token) {
                if (auto dir = direct_choose(0)) {
                    view::copy(dir->stateless_reset_token, token);
                }
            }

            error::Error on_preferred_address_received(CommonParam& cparam, view::rvec connectionID, const StatelessResetToken& token) {
                return accept(cparam, 1, -1, connectionID, token);
            }

            bool initial_conn_id_accepted() const {
                return use_zero_length || active_connid.valid();
            }

            view::rvec pick_up_id(const InitialRetry* iniret) const {
                if (use_zero_length) {
                    return {};
                }
                if (!active_connid.valid()) {
                    return iniret ? iniret->get_initial_or_retry() : view::rvec{};
                }
                return choose(active_connid).id;
            }

            bool has_id(view::rvec cmp) const {
                if (use_zero_length) {
                    return cmp.size() == 0;
                }
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

            bool send(frame::fwriter& fw, auto&& observer) {
                auto do_send = [&](RetireWait& it) {
                    frame::RetireConnectionIDFrame retire;
                    retire.sequence_number = it.seq.seq;
                    if (fw.remain().size() < retire.len()) {
                        return true;  // wait next chance
                    }
                    if (!fw.write(retire)) {
                        return false;
                    }
                    it.wait.wait(observer);
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

            constexpr void on_packet_sent() {
                packet_count_since_id_changed++;
            }
        };

    }  // namespace fnet::quic::connid
}  // namespace futils

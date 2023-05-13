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
#include "config.h"

namespace utils {
    namespace fnet::quic::connid {

        template <class TConfig>
        struct IDManager {
           private:
            Random random;
            IDAcceptor<TConfig> acceptor;
            IDIssuer<TConfig> issuer;
            InitialRetry iniret;

           public:
            void reset(Config&& config) {
                random = std::move(config.random);
                acceptor.reset(config.packet_per_id, config.max_packet_per_id, config.change_mode);
                issuer.reset(std::move(config.exporter), config.connid_len, config.concurrent_limit);
            }

            view::rvec get_initial() {
                return iniret.get_initial();
            }

            view::rvec get_retry() {
                return iniret.get_retry();
            }

            std::pair<view::rvec, error::Error> pick_up_dstID(bool is_server, bool handshake_complete, std::uint64_t local_max_conn_id) {
                auto err = acceptor.maybe_update_id(random, handshake_complete, local_max_conn_id);
                if (err) {
                    return {{}, std::move(err)};
                }
                return {acceptor.pick_up_id(is_server ? nullptr : &iniret), error::none};
            }

            view::rvec pick_up_srcID() {
                return issuer.pick_up_id();
            }

            ConnID choose_dstID(std::int64_t dstID) {
                return acceptor.choose(dstID);
            }

            ConnID choose_srcID(std::int64_t dstID) {
                return issuer.choose(dstID);
            }

            bool local_using_zero_length() const {
                return issuer.is_using_zero_length();
            }

            bool remote_using_zero_length() const {
                return acceptor.is_using_zero_length();
            }

            bool initial_conn_id_accepted() const {
                return acceptor.initial_conn_id_accepted();
            }

            bool gen_initial_random(byte len = 8) {
                return iniret.gen_initial(false, len, random);
            }

            std::pair<ConnID, error::Error> issue(bool enque_wait) {
                return issuer.issue(random, enque_wait);
            }

            error::Error issue_to_max() {
                return issuer.issue_ids_to_max_connid_limit(random);
            }

            error::Error recv_new_connection_id(Version version, const frame::NewConnectionIDFrame& c) {
                return acceptor.recv(version, random, c);
            }

            error::Error recv_retire_connection_id(view::rvec recv_dest_id, const frame::RetireConnectionIDFrame& c) {
                return issuer.retire(random, recv_dest_id, c);
            }

            error::Error recv(packet::PacketSummary summary, const frame::Frame& f) {
                if (f.type.type_detail() == FrameType::NEW_CONNECTION_ID) {
                    return recv_new_connection_id(Version(summary.version), static_cast<const frame::NewConnectionIDFrame&>(f));
                }
                else if (f.type.type_detail() == FrameType::RETIRE_CONNECTION_ID) {
                    return recv_retire_connection_id(summary.dstID, static_cast<const frame::RetireConnectionIDFrame&>(f));
                }
                return error::none;
            }

            IOResult send(frame::fwriter& w, auto&& observer_vec) {
                if (!acceptor.send(w, observer_vec)) {
                    return IOResult::fatal;
                }
                return issuer.send(w, observer_vec) ? IOResult::ok : IOResult::fatal;
            }

            auto get_dstID_len_callback() {
                return issuer.get_onertt_dstID_len_callback();
            }

            auto get_stateless_reset_callback() {
                return acceptor.stateless_reset_callback();
            }

            error::Error on_initial_packet_received(Version version, view::rvec id) {
                if (id.size() == 0) {
                    acceptor.on_zero_length_acception();
                    return error::none;
                }
                byte stateless_reset_token[16]{};
                return acceptor.accept(version, random, 0, 0, id, stateless_reset_token);
            }

            void on_initial_stateless_reset_token_received(const StatelessResetToken& stateless_reset_token) {
                acceptor.on_initial_stateless_reset_token_received(stateless_reset_token);
            }

            error::Error on_preferred_address_received(Version version, view::rvec id, const StatelessResetToken& stateless_reset_token) {
                return acceptor.on_preferred_address_received(version, random, id, stateless_reset_token);
            }

            bool on_retry_received(view::rvec id) {
                return iniret.recv_retry(false, id);
            }

            void on_packet_sent() {
                acceptor.on_packet_sent();
            }

            bool has_dstID(view::rvec id) {
                return acceptor.has_id(id);
            }

            bool has_srcID(view::rvec id) {
                return issuer.has_id(id);
            }
        };
    }  // namespace fnet::quic::connid
}  // namespace utils

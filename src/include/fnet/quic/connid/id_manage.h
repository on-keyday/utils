/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// id_manage - id management
#pragma once
#include "../frame/conn_manage.h"
#include "../transport_error.h"
#include "../ack/ack_lost_record.h"
#include "../ioresult.h"
#include "../frame/writer.h"
#include "config.h"
#include "id_issuer.h"
#include "id_acceptor.h"

namespace futils {
    namespace fnet::quic::connid {

        template <class TConfig>
        struct IDManager {
           private:
            CommonParam cparam;
            IDAcceptor<TConfig> acceptor;
            IDIssuer<TConfig> issuer;
            InitialRetry iniret;

           public:
            void reset(std::uint32_t version, const Config& config) {
                cparam.random = std::move(config.random);
                cparam.version = version;
                acceptor.reset(config.packet_per_id, config.max_packet_per_id, config.change_mode);
                issuer.reset(std::move(config.exporter), config.connid_len, config.concurrent_limit);
                iniret.reset();
            }

            Random& random() {
                return cparam.random;
            }

            void expose_close_ids(auto&& ids) {
                issuer.expose_close_ids(ids);
            }

            view::rvec get_original_dst_id() {
                return iniret.get_initial();
            }

            view::rvec get_retry() {
                return iniret.get_retry();
            }

            std::pair<view::rvec, error::Error> pick_up_dstID(bool is_server, bool handshake_complete, std::uint64_t local_max_conn_id) {
                auto err = acceptor.maybe_update_id(cparam, handshake_complete, local_max_conn_id);
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

            bool gen_original_dst_id() {
                return iniret.gen_initial(false, issuer.get_connID_len(), cparam.random);
            }

            std::pair<ConnID, error::Error> issue(bool enque_wait) {
                return issuer.issue(cparam, enque_wait);
            }

            error::Error issue_to_max() {
                return issuer.issue_ids_to_max_connid_limit(cparam);
            }

            error::Error recv_new_connection_id(const frame::NewConnectionIDFrame& c) {
                return acceptor.recv(cparam, c);
            }

            error::Error recv_retire_connection_id(view::rvec recv_dest_id, const frame::RetireConnectionIDFrame& c) {
                return issuer.retire(cparam, recv_dest_id, c);
            }

            error::Error recv(packet::PacketSummary summary, const frame::Frame& f) {
                if (f.type.type_detail() == FrameType::NEW_CONNECTION_ID) {
                    return recv_new_connection_id(static_cast<const frame::NewConnectionIDFrame&>(f));
                }
                else if (f.type.type_detail() == FrameType::RETIRE_CONNECTION_ID) {
                    return recv_retire_connection_id(summary.dstID, static_cast<const frame::RetireConnectionIDFrame&>(f));
                }
                return error::none;
            }

            IOResult send(frame::fwriter& w, auto&& observer, bool allow_retire) {
                if (allow_retire) {
                    if (!acceptor.send(w, observer)) {
                        return IOResult::fatal;
                    }
                }
                return issuer.send(w, observer)
                           ? IOResult::ok
                           : IOResult::fatal;
            }

            auto get_dstID_len_callback() {
                return issuer.get_onertt_dstID_len_callback();
            }

            auto get_stateless_reset_callback() {
                return acceptor.stateless_reset_callback();
            }

            error::Error on_initial_packet_received(view::rvec id) {
                if (id.size() == 0) {
                    acceptor.on_zero_length_acception();
                    return error::none;
                }
                return acceptor.accept(cparam, 0, 0, id, null_stateless_reset);
            }

            void on_initial_stateless_reset_token_received(const StatelessResetToken& stateless_reset_token) {
                acceptor.on_initial_stateless_reset_token_received(stateless_reset_token);
            }

            error::Error on_preferred_address_received(view::rvec id, const StatelessResetToken& stateless_reset_token) {
                return acceptor.on_preferred_address_received(cparam, id, stateless_reset_token);
            }

            void on_transport_parameter_received(std::uint64_t active_conn) {
                issuer.on_transport_parameter_received(active_conn);
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
}  // namespace futils

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
    namespace fnet::quic::connid {

        struct IDManager {
           private:
            Random random;
            IDAcceptor acceptor;
            IDIssuer issuer;
            InitialRetry iniret;

           public:
            void reset(bool use_zero, Random&& rand) {
                random = std::move(rand);
                acceptor.reset();
                issuer.reset(use_zero);
            }

            view::rvec get_initial() {
                return iniret.get_initial();
            }

            view::rvec get_retry() {
                return iniret.get_retry();
            }

            view::rvec pick_up_dstID(bool is_server) const {
                return acceptor.pick_up_id(is_server ? nullptr : &iniret);
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

            ConnID issue(byte len, bool enque_wait) {
                return issuer.issue(len, random, enque_wait);
            }

            error::Error recv_new_connection_id(Version version, const frame::NewConnectionIDFrame& c) {
                return acceptor.recv(version, c);
            }

            error::Error recv_retire_connection_id(view::rvec recv_dest_id, const frame::RetireConnectionIDFrame& c) {
                return issuer.retire(recv_dest_id, c);
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
                return acceptor.accept(version, 0, 0, id, stateless_reset_token);
            }

            void on_initial_stateless_reset_token_received(const StatelessResetToken& stateless_reset_token) {
                acceptor.on_initial_stateless_reset_token_received(stateless_reset_token);
            }

            error::Error on_preferred_address_received(Version version, view::rvec id, const StatelessResetToken& stateless_reset_token) {
                return acceptor.on_preferred_address_received(version, id, stateless_reset_token);
            }

            bool on_retry_received(view::rvec id) {
                return iniret.recv_retry(false, id);
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

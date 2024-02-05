/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// transport_param_ctx - context suite of transport_parameter
#pragma once
#include "defined_param.h"
#include "../../error.h"
#include "../transport_error.h"
#include "../stream/initial_limits.h"

namespace futils {
    namespace fnet::quic::trsparam {

        struct TransportParameters {
            DefinedTransportParams local;
            DefinedTransportParams peer;
            DuplicateSetChecker peer_checker;

           private:
            // TransportParamStorage local_box;
            // TransportParamStorage peer_box;

           public:
           private:
            error::Error check_recv(std::uint64_t id, bool is_server) {
                if (is_server) {
                    if (is_defined(id) && !is_defined_both_set_allowed(id)) {
                        return QUICError{
                            .msg = "client sent not allowed transport_parameter",
                            .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                        };
                    }
                }
                return error::none;
            }

            error::Error set_(DefinedTransportParams& params, trsparam::DuplicateSetChecker* checker, trsparam::TransportParameter param, bool is_server, bool accept_zero_rtt) {
                if (auto err = check_recv(param.id.id, is_server)) {
                    return err;
                }
                auto [ok, changed] = params.from_transport_param(param, checker);
                if (!ok) {
                    if (checker && checker->detect) {
                        return QUICError{
                            .msg = "duplicate set of transport_parameter detected",
                            .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                        };
                    }
                    return QUICError{
                        .msg = "invalid transport parameter format",
                        .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                    };
                }
                if (changed) {
                    if (auto err = trsparam::to_string(trsparam::validate_transport_param(params))) {
                        return QUICError{
                            .msg = "TransportParameter contract is not satisfied",
                            .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                            .base = error::Error(err, error::Category::lib, error::fnet_quic_transport_parameter_error),
                        };
                    }
                }
                return error::none;
            }

           public:
            error::Error set_local(TransportParameter param) {
                return set_(local, nullptr, param, false, false);
            }

            error::Error set_peer(TransportParameter param, bool is_server, bool accept_zero_rtt) {
                return set_(peer, &peer_checker, param, is_server, accept_zero_rtt);
            }

            error::Error parse_peer(binary::reader& r, bool is_server, bool accept_zero_rtt) {
                while (!r.empty()) {
                    TransportParameter param;
                    if (!param.parse(r)) {
                        return QUICError{
                            .msg = "failed to parse transport parameter",
                            .transport_error = TransportError::TRANSPORT_PARAMETER_ERROR,
                        };
                    }
                    if (auto err = set_peer(param, is_server, accept_zero_rtt)) {
                        return err;
                    }
                }
                return error::none;
            }

            bool render_local(binary::writer& w, bool is_server, bool has_retry) {
                return local.render(w, [&](TransportParameter param) {
                    if (!is_server) {
                        if (!is_defined_both_set_allowed(param.id)) {
                            return false;
                        }
                    }
                    if (!has_retry && param.id == DefinedID::retry_src_connection_id) {
                        return false;
                    }
                    if (is_default_value(param)) {
                        return false;
                    }
                    return true;
                });
            }
        };

        constexpr stream::InitialLimits to_initial_limits(const auto& params) {
            stream::InitialLimits limits;
            limits.conn_data_limit = params.initial_max_data;
            limits.bidi_stream_limit = params.initial_max_streams_bidi;
            limits.uni_stream_limit = params.initial_max_streams_uni;
            limits.bidi_stream_data_local_limit = params.initial_max_stream_data_bidi_local;
            limits.bidi_stream_data_remote_limit = params.initial_max_stream_data_bidi_remote;
            limits.uni_stream_data_limit = params.initial_max_stream_data_uni;
            return limits;
        }
    }  // namespace fnet::quic::trsparam
}  // namespace futils

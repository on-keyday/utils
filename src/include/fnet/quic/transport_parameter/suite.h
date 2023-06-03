/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// transport_param_ctx - context suite of transport_parameter
#pragma once
#include "defined_param.h"
#include "../../error.h"
#include "../transport_error.h"
#include "boxing.h"

namespace utils {
    namespace fnet::quic::trsparam {

        struct TransportParameters {
            DefinedTransportParams local;
            DefinedTransportParams peer;
            TransportParamStorage local_box;
            TransportParamStorage peer_box;
            DuplicateSetChecker peer_checker;

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

            error::Error set_(DefinedTransportParams& params, trsparam::TransportParamStorage& box, trsparam::DuplicateSetChecker* checker, trsparam::TransportParameter param, bool is_server, bool accept_zero_rtt) {
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
                            .base = error::Error(err),
                        };
                    }
                    if (!box.boxing(params)) {
                        return error::memory_exhausted;
                    }
                }
                return error::none;
            }

           public:
            error::Error set_local(TransportParameter param) {
                return set_(local, local_box, nullptr, param, false, false);
            }

            bool boxing() {
                return local_box.boxing(local) && peer_box.boxing(peer);
            }

            error::Error set_peer(TransportParameter param, bool is_server, bool accept_zero_rtt) {
                return set_(peer, peer_box, &peer_checker, param, is_server, accept_zero_rtt);
            }

            error::Error parse_peer(io::reader& r, bool is_server, bool accept_zero_rtt) {
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

            bool render_local(io::writer& w, bool is_server) {
                return local.render(w, [&](TransportParameter param) {
                    if (!is_server) {
                        if (!is_defined_both_set_allowed(param.id)) {
                            return false;
                        }
                    }
                    if (is_default_value(param)) {
                        return false;
                    }
                    return true;
                });
            }
        };
    }  // namespace fnet::quic::trsparam
}  // namespace utils

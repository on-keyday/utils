/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../status/config.h"
#include "../status/new_reno.h"
#include "../version.h"
#include "../../tls/config.h"
#include "../log/log.h"
#include "../connid/config.h"
#include "../transport_parameter/defined_param.h"
#include "../path/config.h"
#include "../dgram/config.h"
#include "../token/token.h"
#include "../../tls/session.h"

namespace futils {
    namespace fnet::quic::context {

        struct Config {
            Version version = version_1;
            bool server = false;
            tls::Session session;  // for 0RTT
            tls::TLSConfig tls_config;
            status::Config internal_parameters;
            path::PathConfig path_parameters;
            trsparam::DefinedTransportParams transport_parameters;
            log::ConnLogger logger;
            connid::Config connid_parameters;
            datagram::Config datagram_parameters;
            token::ZeroRTTStorage zero_rtt;
        };

    }  // namespace fnet::quic::context
}  // namespace futils

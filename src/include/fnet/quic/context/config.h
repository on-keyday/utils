/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../status/config.h"
#include "../version.h"
#include "../../tls/config.h"
#include "../log/log.h"
#include "../connid/random.h"
#include "../transport_parameter/defined_param.h"
#include "../path/config.h"

namespace utils {
    namespace fnet::quic::context {

        struct Config {
            std::uint32_t version = version_1;
            bool server = false;
            tls::TLSConfig tls_config;
            status::Config internal_parameters;
            path::Config path_parameters;
            trsparam::DefinedTransportParams transport_parameters;
            log::Logger logger;
            connid::Random random;
        };

    }  // namespace fnet::quic::context
}  // namespace utils

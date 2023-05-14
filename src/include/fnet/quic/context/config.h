/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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
#include "../stream/config.h"

namespace utils {
    namespace fnet::quic::context {

        struct Config {
            Version version = version_1;
            bool server = false;
            tls::TLSConfig tls_config;
            status::Config internal_parameters;
            path::Config path_parameters;
            trsparam::DefinedTransportParams transport_parameters;
            log::ConnLogger logger;
            connid::Config connid_parameters;
            datagram::Config datagram_parameters;
            token::ZeroRTTStorage zero_rtt;
        };

        template <class CtxLock,
                  class StreamTypeConfig = stream::TypeConfig<CtxLock>,
                  class ConnIDTypeConfig = connid::TypeConfig<>,
                  class CongestionAlgorithm = status::NewReno,
                  class DatagramDrop = datagram::DatagrmDropNull>
        struct TypeConfig {
            using context_lock = CtxLock;
            using stream_type_config = StreamTypeConfig;
            using connid_type_config = ConnIDTypeConfig;
            using congestion_algorithm = CongestionAlgorithm;
            using datagram_drop = DatagramDrop;
            struct user_defined_types_config {
                congestion_algorithm algorithm;
                datagram_drop dgram_drop;
            };
        };

    }  // namespace fnet::quic::context
}  // namespace utils

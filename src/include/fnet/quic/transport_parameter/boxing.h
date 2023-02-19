/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "defined_param.h"
#include "../stream/initial_limits.h"

namespace utils {
    namespace fnet::quic::trsparam {
        // TransportParamStorage is boxing utility of DefinedTransportParams
        struct TransportParamStorage {
            storage boxed[4];

           private:
            bool do_boxing(storage& box, view::rvec& param) {
                if (!param.null() && box.data() != param.data()) {
                    box = make_storage(param);
                    if (box.null()) {
                        return false;
                    }
                    param = box;
                }
                return true;
            }

           public:
            bool boxing(DefinedTransportParams& params) {
                return do_boxing(boxed[0], params.original_dst_connection_id) &&
                       do_boxing(boxed[1], params.preferred_address.connectionID) &&
                       do_boxing(boxed[2], params.initial_src_connection_id) &&
                       do_boxing(boxed[3], params.retry_src_connection_id);
            }
        };

        constexpr stream::InitialLimits to_initial_limits(const DefinedTransportParams& params) {
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
}  // namespace utils

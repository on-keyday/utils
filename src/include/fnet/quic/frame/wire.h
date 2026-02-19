/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "datagram.h"
#include "crypto.h"
#include "stream_manage.h"
#include "path.h"
#include "conn_manage.h"
#include "ack.h"
#include "token.h"
#include "conn_id.h"

namespace futils::fnet::quic::frame {
    static_assert(test::check_padding());
    static_assert(test::check_dgram());
    static_assert(test::check_crypto());
    static_assert(test::check_reset_stream());
    static_assert(test::check_stop_sending());
    static_assert(test::check_stream());
    static_assert(test::check_max_stream_data());
    static_assert(test::check_path());
    static_assert(test::check_new_token());
    static_assert(test::check_max_data());
    static_assert(test::check_max_streams());
    static_assert(test::check_new_connection_id());
    static_assert(test::check_retire_connection_id());
    static_assert(test::check_connection_close());
    static_assert(test::check_ack());
}  // namespace futils::fnet::quic::frame

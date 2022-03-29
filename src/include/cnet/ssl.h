/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ssl - cnet ssl context
#pragma once
#include "cnet.h"

namespace utils {
    namespace cnet {
        namespace ssl {
            DLL CNet* STDCALL create_client();

            enum TLSConfig {
                // alpn string
                alpn,
                // host name for verification
                host_verify,
                // certificate file path
                cert_file,

                // raw ssl context (get only)
                raw_ssl,
            };

            enum class TLSStatus {
                idle,
                start_write_to_low,
                writing_to_low,
                write_to_low_done,

                start_read_from_low,
                reading_from_low,
                read_from_low_done,

                start_connect,
                connected,

                start_read,
                read_done,

                start_write,
                write_done,
            };
        }  // namespace ssl
    }      // namespace cnet
}  // namespace utils

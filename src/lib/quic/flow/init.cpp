/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/core/core.h>
#include <quic/internal/context_internal.h>
#include <quic/conn/conn.h>
#include <cassert>

namespace utils {
    namespace quic {
        namespace flow {
            enum class Error {
                memory_exhausted,
            };

            Error start_conn_client(core::QUIC* q, io::Target target) {
                assert(q);
                auto c = conn::new_connection(q);
                if (!c) {
                    return Error::memory_exhausted;
                }
            }
        }  // namespace flow
    }      // namespace quic
}  // namespace utils

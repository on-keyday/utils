/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// context_internal - context internal structs and methods
#include "../context.h"
#include "../doc.h"
#include "alloc.h"
#include "pool.h"
#include "callbacks.h"
#include "../conn/connId.h"
#include "../io/io.h"

namespace utils {
    namespace quic {
        namespace context {

            struct QUIC {
                // alloc manages all allocation related with this QUIC object
                allocate::Alloc alloc;

                // global callback
                callback::CommonCallbacks callback;
                callback::GlobalCallbacks gcallback;

                // bpool manages byte sequence object pool
                pool::BytesPool bpool;
            };

            struct Connection {
                // connection callback
                callback::CommonCallbacks callback;

                // parent QUIC object of this Connection
                QUIC* q;

                // connection mode
                Mode mode;

                // srcIDs holds connection IDs issued by this side
                conn::ConnIDList srcIDs;
            };

            struct Stream {
            };

        }  // namespace context
    }      // namespace quic
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// context_internal - context internal structs and methods
#include "../context.h"
#include "../doc.h"
#include "../mem/alloc.h"
#include "../mem/pool.h"
#include "../mem/callbacks.h"
#include "../conn/connId.h"
#include "../io/io.h"
#include "../mem/que.h"
#include "../mem/shared_ptr.h"
#include "../core/event.h"
#include "../mem/once.h"
#include "../io/async.h"

namespace utils {
    namespace quic {

        namespace core {
            struct EventQue {
                byte opaque[sizeof(mem::LinkQue<Event>) + sizeof(Event)];  // sizeof(mem::Que<Event>)+sizeof(Event)
            };

            struct EQue;

            using events = mem::shared_ptr<EQue>;

            struct Global {
                // alloc manages all allocation with this Global
                allocate::Alloc alloc;
                // global callback
                callback::CommonCallbacks callback;
                callback::GlobalCallbacks gcallback;
                // bpool manages byte sequence object pool
                pool::BytesPool bpool;
            };

            using global = mem::shared_ptr<Global>;

            struct IOHandles {
                // normal io procs
                io::IO ioproc;

                mem::Once iniasync;
                // async io handle
                io::AsyncHandle async_handle;
            };

            using io_handles = mem::shared_ptr<IOHandles>;

            struct QUIC {
                // global object
                global g;

                // ev holds event related with this context
                events ev;

                // io_handles holds io operation handles
                io_handles io;
            };

        }  // namespace core

        namespace context {

            struct Connection {
                // connection callback
                callback::CommonCallbacks callback;

                // parent QUIC object of this Connection
                core::QUIC* q;

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

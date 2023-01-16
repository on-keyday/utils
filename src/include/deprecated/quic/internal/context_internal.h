/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// context_internal - context internal structs and methods
#pragma once
#include "../doc.h"
#include "../mem/alloc.h"
#include "../mem/pool.h"
#include "../mem/event_callback.h"
#include "../conn/connId.h"
#include "../io/io.h"
#include "../mem/que.h"
#include "../mem/shared_ptr.h"
#include "../core/event.h"
#include "../mem/once.h"
#include "../io/async.h"

namespace utils {
    namespace quic {

        namespace conn {
            struct Connection;
            using Conn = mem::shared_ptr<Connection>;
            struct ConnPool {
                using QueT = mem::LinkQue<Conn, mem::SharedStock<Conn, std::recursive_mutex>>;
                mem::StockNode<Conn, std::recursive_mutex> stock;
                QueT conns;  // conns holds pooled connections
                QueT alive;  // alive holds alive connections
                Conn get();
            };

        }  // namespace conn

        namespace core {

            struct EQue {
                mem::LinkQue<Event> que;
                Event mem_exhausted;
                bool en_q(Event&& e);
                Event de_q();
            };

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

                // self allocated
                mem::shared_ptr<QUIC> self;

                conn::ConnPool conns;
            };

        }  // namespace core

        namespace conn {

            struct Connection {
                // connection callback
                callback::CommonCallbacks callback;

                // parent QUIC object of this Connection
                mem::shared_ptr<core::QUIC> q;

                // connection mode
                Mode mode;

                // srcIDs holds connection IDs issued by this side
                conn::ConnIDList srcIDs;

                conn::Conn self;

                // ssl library context
                void* ssl;
                void* sslctx;
                byte alert;  // alert ssl

                // temporary ssl send data callback holder
                mem::CBDef<int, 1 /*succeeded*/, void* /*app data*/> tmp_callback;
            };

            struct Stream {
            };

        }  // namespace conn
    }      // namespace quic
}  // namespace utils

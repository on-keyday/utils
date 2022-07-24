/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// core - core quic loops
#pragma once
#include "../common/dll_h.h"
#include "../mem/event_callback.h"
#include "../mem/alloc.h"
#include "../mem/pool.h"
#include "../mem/shared_ptr.h"
#include "../io/io.h"
#include "event.h"

namespace utils {
    namespace quic {
        namespace core {
            // QUIC manages all of this library context
            struct QUIC;

            // EventLoop manages quic loop context
            struct EventLoop;

            // progress_loop progresses the loop of QUIC connection
            // progress_loop doesn't block operation
            Dll(bool) progress_loop(EventLoop* l);

            Dll(void) enque_event(QUIC* q, Event e);
            Dll(QUIC*) new_QUIC(allocate::Alloc a);
            Dll(QUIC*) default_QUIC();
            Dll(void) set_bpool(QUIC* q, pool::BytesHolder h);
            Dll(EventLoop*) new_Looper(QUIC* q);
            Dll(bool) add_loop(EventLoop* l, QUIC* q);
            Dll(tsize) rem_loop(EventLoop* l, QUIC* q);
            Dll(allocate::Alloc*) get_alloc(QUIC* q);
            Dll(void) register_io(QUIC* q, io::IO io);

            Dll(io::IO*) get_io(QUIC* q);

            Dll(bytes::Bytes) get_bytes(QUIC* q, tsize len);
            Dll(void) put_bytes(QUIC* q, bytes::Bytes&& b);

        }  // namespace core

        namespace bytes {
            Dll(pool::BytesHolder) stdbpool(core::QUIC* q);
        }

    }  // namespace quic
}  // namespace utils

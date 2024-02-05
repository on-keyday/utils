/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../json/stringer.h"
#include "state.h"

namespace futils {
    namespace fnet::server {
        template <class String>
        String format_state(const Counter& servstate) {
            json::Stringer<String> w;
            w.set_indent("    ");
            {
                auto field = w.object();
                field("current_acceptor_thread", servstate.current_acceptor_thread.load());
                field("current_handler_thread", servstate.current_handler_thread.load());
                field("current_handling_handler_thread", servstate.current_handling_handler_thread.load());
                field("waiting_async_read", servstate.waiting_async_read.load());
                field("total_async_invocation", servstate.total_async_invocation.load());
                field("max_concurrent_async_invocation", servstate.max_concurrent_async_invocation.load());
                field("total_accepted", servstate.total_accepted.load());
                field("active_recommend", servstate.waiting_recommend.load());
                field("max_handler_thread", servstate.max_handler_thread.load());
                field("total_failed_accept", servstate.total_failed_accept.load());
                field("max_launched_handler_thread", servstate.max_launched_handler_thread.load());
                field("total_failed_async", servstate.total_failed_async.load());
                field("thread_sleep", servstate.thread_sleep.load());
                field("reduce_skip", servstate.reduce_skip.load());
                field("total_launched_handler_thread", servstate.total_launched_handler_thread.load());
                field("current_enqueued", servstate.current_enqueued.load());
            }
            return w.out();
        }
    }  // namespace fnet::server
}  // namespace futils

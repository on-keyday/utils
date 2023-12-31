/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../json/to_string.h"
#include "state.h"

namespace utils {
    namespace fnet::server {
        template <class String, class JSON>
        String format_state(const Counter& servstate, JSON& js) {
            js["current_acceptor_thread"] = servstate.current_acceptor_thread.load();
            js["current_handler_thread"] = servstate.current_handler_thread.load();
            js["current_handling_handler_thread"] = servstate.current_handling_handler_thread.load();
            js["waiting_async_read"] = servstate.waiting_async_read.load();
            js["total_async_invocation"] = servstate.total_async_invocation.load();
            js["max_concurrent_async_invocation"] = servstate.max_concurrent_async_invocation.load();
            js["total_accepted"] = servstate.total_accepted.load();
            js["active_recommend"] = servstate.waiting_recommend.load();
            js["max_handler_thread"] = servstate.max_handler_thread.load();
            js["total_failed_accept"] = servstate.total_failed_accept.load();
            js["max_launched_handler_thread"] = servstate.max_launched_handler_thread.load();
            js["total_failed_async"] = servstate.total_failed_async.load();
            js["thread_sleep"] = servstate.thread_sleep.load();
            js["reduce_skip"] = servstate.reduce_skip.load();
            js["total_launched_handler_thread"] = servstate.total_launched_handler_thread.load();
            js["current_enqueued"] = servstate.current_enqueued.load();
            return json::to_string<String>(js, json::FmtFlag::last_line);
        }
    }  // namespace fnet::server
}  // namespace utils

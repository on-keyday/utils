/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// external
#include <dnet/server/servcpp.h>
#include <dnet/socket.h>
#include <thread>
#include <dnet/server/state.h>

namespace utils {
    namespace dnet {
        namespace server {

            void State::handler_thread(std::shared_ptr<State> state) {
                auto recv = state->recv;
                recv.set_blocking(false);
                auto deq = state->deque;
                deq.set_blocking(false);
                auto decr_count = helper::defer([&] {
                    state->count.current_handler_thread--;
                });
                std::uint32_t skip = 0;
                while (!state->count.end_flag.test()) {
                    Client cl;
                    wait_io_event(1);
                    Queued q;
                    if (deq >> q) {
                        state->count.current_enqued--;
                        Enter active(state->count.current_handling_handler_thread);
                        q.runner->run();
                    }
                    auto res = recv >> cl;
                    if (!res) {
                        if (state->count.should_reduce()) {
                            if (skip >= state->count.reduce_skip) {
                                auto c_thread = state->count.current_handler_thread.load();
                                if (state->count.current_handler_thread.compare_exchange_strong(c_thread, c_thread - 1)) {
                                    decr_count.cancel();  // stop decrement
                                    return;
                                }
                            }
                            else {
                                skip++;
                            }
                        }
                        if (state->count.thread_sleep == 0) {
                            std::this_thread::yield();
                        }
                        else {
                            std::this_thread::sleep_for(std::chrono::milliseconds(state->count.thread_sleep));
                        }
                        continue;
                    }
                    skip = 0;
                    Enter active(state->count.current_handling_handler_thread);
                    if (state->handler) {
                        state->handler(state->ctx, std::move(cl), StateContext{state});
                    }
                }
            }

            void State::accept_thread(Socket listener, std::shared_ptr<State> state) {
                Enter start{state->count.current_acceptor_thread};
                while (!state->count.end_flag.test()) {
                    auto handle = [&](Socket&& sock, NetAddrPort&& addr) {
                        state->count.total_accepted++;
                        state->check_and_start();
                        state->send << Client{std::move(sock), std::move(addr)};
                    };
                    do_accept(listener, handle, [&](error::Error&, bool waiting) {
                        if (!waiting) {
                            state->count.total_failed_accept++;
                        }
                        return waiting;
                    });
                    std::this_thread::yield();
                }
            }

            bool State::serve(Socket& listener, bool (*cb)(void*), void* user) {
                if (!listener) {
                    return false;
                }
                auto deq = deque;
                deq.set_blocking(false);
                while (true) {
                    wait_io_event(1);
                    Queued q;
                    if (deq >> q) {
                        count.current_enqued--;
                        Enter active(count.current_handling_handler_thread);
                        q.runner->run();
                    }
                    auto handle = [&](Socket&& sock, NetAddrPort&& addr) {
                        count.total_accepted++;
                        Enter active(count.current_handling_handler_thread);
                        if (handler) {
                            handler(ctx, Client{std::move(sock), std::move(addr)}, StateContext{shared_from_this()});
                        }
                    };
                    do_accept(listener, handle, [&](error::Error& err, bool waiting) {
                        if (!waiting) {
                            count.total_failed_accept++;
                        }
                        return waiting;
                    });
                    if (cb) {
                        if (!cb(user)) {
                            return true;
                        }
                    }
                }
            }

            void State::check_and_start() {
                if (count.should_start()) {
                    auto c_thread = count.current_handler_thread.load();
                    if (count.current_handler_thread.compare_exchange_strong(c_thread, c_thread + 1)) {
                        std::thread(handler_thread, shared_from_this()).detach();
                        count.max_launched_handler_thread.compare_exchange_strong(c_thread, c_thread + 1);
                        count.total_launched_handler_thread++;
                    }
                }
            }

            void State::add_accept_thread(Socket&& s) {
                if (!s) {
                    return;
                }
                std::thread(accept_thread, std::move(s), shared_from_this()).detach();
            }

        }  // namespace server
    }      // namespace dnet
}  // namespace utils

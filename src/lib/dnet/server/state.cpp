/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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
                Enter start(state->count.current_handler_thread);
                std::uint32_t curcount = state->count.current_handler_thread;
                auto to = curcount;
                if (curcount) {
                    curcount--;
                }
                state->count.max_launched_handler_thread.compare_exchange_strong(curcount, to);
                while (!state->count.end_flag.test()) {
                    Client cl;
                    wait_event(1);
                    auto res = recv >> cl;
                    if (!res) {
                        if (state->count.should_reduce()) {
                            return;  // reduce
                        }
                        std::this_thread::yield();
                        continue;
                    }
                    Enter active(state->count.current_handling_handler_thread);
                    if (state->handler) {
                        state->handler(state->ctx, std::move(cl), StateContext{state});
                    }
                }
            }

            void State::accept_thread(Socket listener, std::shared_ptr<State> state) {
                Enter start{state->count.current_acceptor_thread};
                while (!state->count.end_flag.test()) {
                    auto handle = [&](Socket& sock, IPText addr, int port) {
                        state->count.total_accepted++;
                        state->check_and_start();
                        state->send << Client{std::move(sock), addr, port};
                    };
                    do_accept(listener, handle, [&](bool waiting) {
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
                while (true) {
                    wait_event(1);
                    auto handle = [&](Socket& sock, IPText addr, int port) {
                        count.total_accepted++;
                        Enter active(count.current_handling_handler_thread);
                        if (handler) {
                            handler(ctx, Client{std::move(sock), addr, port}, StateContext{shared_from_this()});
                        }
                    };
                    do_accept(listener, handle, [&](bool waiting) {
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
                    std::thread(handler_thread, shared_from_this()).detach();
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

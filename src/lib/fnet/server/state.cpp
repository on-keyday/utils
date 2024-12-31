/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// external
#include <fnet/server/servcpp.h>
#include <fnet/socket.h>
#include <thread>
#include <fnet/server/state.h>

namespace futils {
    namespace fnet {
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
                        state->count.current_enqueued--;
                        Enter active(state->count.current_handling_handler_thread);
                        q.runner.invoke();
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
                auto handle = [&](Socket&& sock, NetAddrPort&& addr) {
                    state->count.total_accepted++;
                    state->check_and_start();
                    state->send << Client{std::move(sock), std::move(addr)};
                };
                while (!state->count.end_flag.test()) {
                    auto new_socks = listener.accept_select(0, 1000);
                    if (new_socks) {
                        handle(std::move(new_socks->first), std::move(new_socks->second));
                    }
                    else if (!isSysBlock(new_socks.error())) {
                        state->count.total_failed_accept++;
                        state->log(log_level::warn, nullptr, new_socks.error());
                    }
                    std::this_thread::yield();
                }
            }

            bool State::serve(expected<std::pair<Socket, NetAddrPort>> (*listener)(void*), void* listener_p, bool (*cb)(void*), void* user) {
                if (!listener) {
                    return false;
                }
                auto deq = deque;
                deq.set_blocking(false);
                auto handle = [&](Socket&& sock, NetAddrPort&& addr) {
                    count.total_accepted++;
                    Enter active(count.current_handling_handler_thread);
                    if (handler) {
                        handler(ctx, Client{std::move(sock), std::move(addr)}, StateContext{shared_from_this()});
                    }
                };
                while (true) {
                    wait_io_event(1);
                    Queued q;
                    if (deq >> q) {
                        count.current_enqueued--;
                        Enter active(count.current_handling_handler_thread);
                        q.runner.invoke();
                    }
                    auto new_socks = listener(listener_p);
                    if (new_socks) {
                        handle(std::move(new_socks->first), std::move(new_socks->second));
                    }
                    else if (!isSysBlock(new_socks.error())) {
                        count.total_failed_accept++;
                        log(log_level::warn, nullptr, new_socks.error());
                    }
                    if (cb) {
                        if (!cb(user)) {
                            return true;
                        }
                    }
                }
            }

            void State::check_and_start() {
                if (count.should_start()) {
                    auto c = ++count.current_handler_thread;
                    std::thread(handler_thread, shared_from_this()).detach();
                    c -= 1;
                    count.max_launched_handler_thread.compare_exchange_strong(c, c + 1);
                    count.total_launched_handler_thread++;
                }
            }

            void State::add_accept_thread(Socket&& s) {
                if (!s) {
                    return;
                }
                std::thread(accept_thread, std::move(s), shared_from_this()).detach();
            }

        }  // namespace server
    }  // namespace fnet
}  // namespace futils

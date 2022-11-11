/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// state - server runtime state
#pragma once
// external
#include "servh.h"
#include "accept.h"
#include "client.h"
#include "../dll/allocator.h"
#include <atomic>
#include <cstdint>
#include "../../thread/channel.h"
#include <memory>

namespace utils {
    namespace dnet {
        namespace server {
            struct Counter {
                // maximum concurrent launched handler thread
                std::atomic_uint32_t max_launched_handler_thread;
                // current request handling handler thread
                std::atomic_uint32_t current_handling_handler_thread;
                // current handler thread
                std::atomic_uint32_t current_handler_thread;
                // current acceptor thread
                std::atomic_uint32_t current_acceptor_thread;
                // current waiting IOCP/epoll call count
                std::atomic_uint32_t waiting_async_read;
                // current enqueued count
                std::atomic_uint32_t current_enqued;
                // total IOCP/epoll call count
                std::atomic_uint64_t total_async_invocation;
                // total accept called
                std::atomic_uint64_t total_accepted;
                // total accept failure
                std::atomic_uint64_t total_failed_accept;
                // total IOCP/epoll call failure
                std::atomic_uint64_t total_failed_async;
                // total queued user defined operation
                std::atomic_uint64_t total_queued;

                // limit maximum thread launchable
                std::atomic_uint32_t max_handler_thread;
                // recommended waiting handler thread count
                std::atomic_uint32_t waiting_recommend;
                std::atomic_flag end_flag;
                // waiting_handler is count of request waiting handler thread
                std::uint32_t waiting_handler() {
                    return current_handler_thread - current_handling_handler_thread;
                }

                bool should_start() {
                    return waiting_handler() < waiting_recommend &&
                           current_handler_thread < max_handler_thread;
                }

                bool should_reduce() {
                    return waiting_handler() > waiting_recommend;
                }
            };

            template <class T>
            struct Enter {
                T& count;
                explicit Enter(T& c)
                    : count(c) {
                    count++;
                }
                ~Enter() {
                    count--;
                }
            };

            struct StateContext;

            enum log_level {
                perf,
                debug,
                normal,
            };

            struct Queued {
                void (*fn)(std::shared_ptr<void>&& ptr, StateContext&&);
                std::shared_ptr<void> ptr;
            };

            struct dnetserv_class_export State : std::enable_shared_from_this<State> {
                using log_t = void (*)(log_level, const char* msg, Client&,
                                       const char* data, size_t len);

               private:
                Counter count;
                thread::SendChan<Client> send;
                thread::RecvChan<Client> recv;
                thread::SendChan<Queued> enque;
                thread::RecvChan<Queued> deque;
                void (*handler)(void*, Client&&, StateContext);
                void* ctx;
                log_t log_evt = nullptr;

                void check_and_start();
                static void handler_thread(std::shared_ptr<State> state);
                static void accept_thread(Socket, std::shared_ptr<State> state);
                friend struct StateContext;

               public:
                State(void* c, void (*h)(void*, Client&&, StateContext))
                    : handler(h), ctx(c) {
                    auto [w, r] = thread::make_chan<Client>();
                    send = w;
                    recv = r;
                    auto [eq, dq] = thread::make_chan<Queued>();
                    enque = eq;
                    deque = dq;
                    count.waiting_recommend = 3;
                    count.max_handler_thread = 12;
                }
                void set_log(log_t ev) {
                    log_evt = ev;
                }

                // add_accept_thread launches new accept_thread with listener
                // accept_thread launches handler_thread on demand
                // and invokes user defined handlers
                void add_accept_thread(Socket&& listener);

                const Counter& state() const {
                    return count;
                }

                void set_max_and_active(size_t max_thread, size_t waiting_recommend) {
                    count.max_handler_thread = max_thread;
                    count.waiting_recommend = waiting_recommend;
                }

                void notify(bool end = true) {
                    if (end) {
                        count.end_flag.test_and_set();
                    }
                    else {
                        count.end_flag.clear();
                    }
                }

                // serve serves request with listener
                // this function is not return until notify() is called
                // cb(user) is called on each loop
                bool serve(Socket& listener, bool (*cb)(void*), void* user);

                // this function is easy wrapper of original serve function
                bool serve(Socket& listener, auto&& fn) {
                    auto ptr = std::addressof(fn);
                    return serve(
                        listener, [](void* p) -> bool {
                            auto& call = (*decltype(ptr)(p));
                            if constexpr (std::is_same_v<decltype(call()), void>) {
                                call();
                                return true;
                            }
                            else {
                                return bool(call());
                            }
                        },
                        ptr);
                }

               private:
                bool read_async(Socket& sock, auto fn, auto&& object, auto get_sock, auto add_data) {
                    count.total_async_invocation++;
                    count.waiting_async_read++;
                    if (auto err = sock.read_async(
                            [th = shared_from_this(), fn](auto&& obj, error::Error err) {
                                th->count.waiting_async_read--;
                                Enter ent{th->count.current_handling_handler_thread};
                                fn(std::move(obj), {th});
                            },
                            std::forward<decltype(object)>(object), get_sock, add_data)) {
                        count.total_async_invocation--;
                        count.waiting_async_read--;
                        count.total_failed_async++;
                        return false;
                    }
                    return true;
                }

                void enque_object(auto fn, auto&& obj) {
                    count.total_queued++;
                    count.current_enqued++;
                    enque << Queued{fn, obj};
                }
            };

            struct StateContext {
               private:
                std::shared_ptr<State> s;
                friend struct State;
                StateContext(const std::shared_ptr<State>& s)
                    : s(s) {}

               public:
                bool read_async(Socket& sock, auto fn, auto&& object, auto get_sock, auto add_data) {
                    return s->read_async(sock, std::forward<decltype(fn)>(fn),
                                         std::forward<decltype(object)>(object),
                                         std::forward<decltype(get_sock)>(get_sock),
                                         std::forward<decltype(add_data)>(add_data));
                }

                void log(log_level level, const char* msg, Client& cl, const char* data = nullptr, size_t len = 0) {
                    auto l = s->log_evt;
                    if (l) {
                        l(level, msg, cl, data, len);
                    }
                }

                void enque_object(auto fn, auto&& obj) {
                    struct Internal {
                        std::decay_t<decltype(fn)> fn;
                        std::remove_cvref_t<decltype(obj)> obj;
                    };
                    glheap_allocator<Internal> gl;
                    auto inobj = std::allocate_shared<Internal>(gl);
                    inobj->fn = std::forward<decltype(fn)>(fn);
                    inobj->obj = std::forward<decltype(obj)>(obj);
                    auto f = [](std::shared_ptr<void>&& obj, StateContext&& st) {
                        auto in = std::static_pointer_cast<Internal>(obj);
                        in->fn(std::move(in->obj), std::move(st));
                    };
                    s->enque_object(f, std::move(inobj));
                }
            };

            inline dnet::Socket& get_client_sock(Client& s) {
                return s.sock;
            }

        }  // namespace server
    }      // namespace dnet
}  // namespace utils

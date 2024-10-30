/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include <fnet/quic/server/server.h>
#include <fnet/quic/quic.h>

namespace futils {
    namespace fnet {
        namespace server {
            struct Counter {
                std::atomic_flag end_flag;

                // maximum concurrent launched handler thread
                std::atomic_uint32_t max_launched_handler_thread;
                // current request handling handler thread
                std::atomic_uint32_t current_handling_handler_thread;
                // current handler thread
                std::atomic_uint32_t current_handler_thread;
                // current acceptor thread
                std::atomic_uint32_t current_acceptor_thread;
                // current waiting IOCP/epoll read call count
                std::atomic_uint32_t waiting_async_read;
                // current waiting IOCP/epoll write call count
                std::atomic_uint32_t waiting_async_write;
                // current enqueued count
                std::atomic_uint32_t current_enqueued;
                // total IOCP/epoll read call count
                std::atomic_uint64_t total_async_read_invocation;
                // total IOCP/epoll write call count
                std::atomic_uint64_t total_async_write_invocation;
                // max concurrent IOCP/epoll read call count (estimated)
                std::atomic_uint32_t max_concurrent_async_read_invocation;
                // max concurrent IOCP/epoll write call count (estimated)
                std::atomic_uint32_t max_concurrent_async_write_invocation;
                // total accept called
                std::atomic_uint64_t total_accepted;
                // total accept failure
                std::atomic_uint64_t total_failed_accept;
                // total IOCP/epoll read call failure
                std::atomic_uint64_t total_failed_read_async;
                // total IOCP/epoll write call failure
                std::atomic_uint64_t total_failed_write_async;
                // total queued user defined operation
                std::atomic_uint64_t total_queued;
                // total launched thread count
                std::atomic_uint64_t total_launched_handler_thread;

                // limit maximum thread launchable
                std::atomic_uint32_t max_handler_thread;
                // recommended waiting handler thread count
                std::atomic_uint32_t waiting_recommend;
                // sleep for (millisecond)
                std::atomic_uint64_t thread_sleep;
                // max skip reducing chance
                std::atomic_uint32_t reduce_skip;

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

                bool busy() const {
                    return current_handling_handler_thread > waiting_recommend;
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

            enum class log_level {
                perf,
                debug,
                info,
                warn,
                err,
            };

            DEFINE_ENUM_COMPAREOP(log_level)

            struct Queued {
                DeferredCallback runner;
            };

            using ServerEntry = void (*)(void*, Client&&, StateContext);

            struct State;

            struct QUICServerState {
                std::weak_ptr<State> state;
                fnet::quic::server::MultiplexerConfig<fnet::quic::use::smartptr::DefaultTypeConfig> original_config;
                std::weak_ptr<fnet::quic::server::HandlerMap<fnet::quic::use::smartptr::DefaultTypeConfig>> handler;
            };

            struct fnetserv_class_export State : std::enable_shared_from_this<State> {
                using log_t = void (*)(log_level, NetAddrPort* addr, error::Error& err);
                using quic_handler = fnet::quic::server::HandlerMap<fnet::quic::use::smartptr::DefaultTypeConfig>;
                using quic_server_config = fnet::quic::server::MultiplexerConfig<fnet::quic::use::smartptr::DefaultTypeConfig>;

               private:
                Counter count;
                thread::SendChan<Client> send;
                thread::RecvChan<Client> recv;
                thread::SendChan<Queued> io_notify;
                thread::RecvChan<Queued> deque;
                ServerEntry handler;
                void* ctx;
                log_t log_evt = nullptr;

                void check_and_start();
                static void handler_thread(std::shared_ptr<State> state);
                static void accept_thread(Socket, std::shared_ptr<State> state);

                static void quic_send_thread(std::shared_ptr<State> state, fnet::Socket s, std::shared_ptr<quic_handler> handler);
                static void quic_recv_handler(std::shared_ptr<State> state, fnet::Socket s, std::shared_ptr<quic_handler> handler);
                static void quic_send_scheduler(std::shared_ptr<State> state, std::shared_ptr<quic_handler> handler);
                static void quic_recv_scheduler(std::shared_ptr<State> state, std::shared_ptr<quic_handler> handler);
                friend struct StateContext;

               public:
                State(void* c, ServerEntry entry)
                    : handler(entry), ctx(c) {
                    auto [w, r] = thread::make_chan<Client>();
                    send = w;
                    recv = r;
                    auto [eq, dq] = thread::make_chan<Queued>();
                    io_notify = eq;
                    deque = dq;
                    count.waiting_recommend = 3;
                    count.max_handler_thread = 12;
                }

                // for http3 handler
                void* get_ctx() const {
                    return ctx;
                }

                void set_log(log_t ev) {
                    log_evt = ev;
                }

                // add_accept_thread launches new accept_thread with listener
                // accept_thread launches handler_thread on demand
                // and invokes user defined handlers
                void add_accept_thread(Socket&& listener);

                // add_quic_thread launches new quic_send_thread, quic_send_scheduler, quic_recv_scheduler and start quic_recv_handler
                // some fields of serv_conf will be modified
                // serv_conf.app_ctx will be set to QUICServerState with this pointer and original_config
                void add_quic_thread(Socket&& listener, std::shared_ptr<quic_handler> handler,
                                     fnet::quic::context::Config&& conf,
                                     quic_server_config serv_conf);

                const Counter& state() const {
                    return count;
                }

                void set_max_and_active(size_t max_thread, size_t waiting_recommend) {
                    count.max_handler_thread = max_thread;
                    count.waiting_recommend = waiting_recommend;
                }

                void set_thread_sleep(std::uint64_t milli) {
                    count.thread_sleep = milli;
                }

                void set_reduce_skip(std::uint32_t skip) {
                    count.reduce_skip = skip;
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
                bool serve(expected<std::pair<Socket, NetAddrPort>> (*listener)(void*), void* listener_p, bool (*cb)(void*), void* user);

                // this function is easy wrapper of original serve function
                bool serve(Socket& listener, auto&& fn) {
                    auto ptr = std::addressof(fn);
                    return serve(
                        [](void* p) {
                            auto s = (Socket*)p;
                            return s->accept_select(0, 1000);
                        },
                        &listener,
                        [](void* p) -> bool {
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

                // this function is easy wrapper of original serve function
                bool serve(auto&& listener, auto&& fn) {
                    auto ptr = std::addressof(fn);
                    return serve(
                        [](void* p) {
                            return (*decltype(std::addressof(listener))(p))();
                        },
                        std::addressof(listener),
                        [](void* p) -> bool {
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

                void log(log_level level, NetAddrPort* addr, error::Error& err) {
                    if (log_evt) {
                        log_evt(level, addr, err);
                    }
                }

               private:
                auto async_read_callback(auto&& fn, auto&& context) {
                    return async_notify_then(
                        std::forward<decltype(context)>(context),
                        // HACK(on-keyday): in this case, this pointer is valid via th of runner function
                        // so use this pointer instead of shared_from_this()
                        [this](DeferredCallback&& n) { this->enqueue_callback(std::move(n)); },
                        [th = shared_from_this(), fn](Socket&& s, auto&& context, NotifyResult&& r) mutable {
                            th->count.waiting_async_read--;
                            auto& result = r.value();
                            if (result) {
                                auto& size = *result;
                                auto&& base_buffer = context.get_buffer();
                                if (size) {
                                    context.add_data(view::wvec(base_buffer).substr(0, *size), {th});
                                }
                                if (!size || *size == base_buffer.size()) {
                                    result = s.read_until_block(base_buffer, [&](view::rvec s) {
                                        context.add_data(s, {th});
                                    });
                                }
                            }
                            if (!result) {
                                if (th->log_evt) {
                                    auto addr = s.get_remote_addr();
                                    th->log_evt(log_level::warn, addr.value_ptr(), result.error());
                                }
                            }
                            fn(std::move(s), std::move(context), {th}, (bool)result.error_ptr());
                        });
                }

                // fn is void(Socket&& sock,auto&& context,StateContext&&)
                // context.get_buffer() returns view::wvec for reading buffer
                // context.add_data(view::rvec,StateContext) appends application data
                bool read_async(Socket& sock, auto fn, auto&& context) {
                    // use explicit clone
                    // context may contains sock self, and then read_async moves sock into heap and
                    // finally fails to call read_async
                    // so use cloned sock
                    auto s = sock.clone();
                    auto result = s.read_async_deferred(async_read_callback(std::forward<decltype(fn)>(fn), std::forward<decltype(context)>(context)));
                    if (!result) {
                        count.total_failed_read_async++;
                        auto addr = s.get_remote_addr();
                        log(log_level::warn, addr.value_ptr(), result.error());
                        return false;
                    }
                    count.total_async_read_invocation++;
                    auto concur = ++count.waiting_async_read - 1;
                    count.max_concurrent_async_read_invocation.compare_exchange_strong(concur, concur + 1);
                    return true;
                }

                auto async_write_callback(auto&& fn, auto&& context) {
                    return async_notify_then(
                        std::forward<decltype(context)>(context),
                        // HACK(on-keyday): in this case, this pointer is valid via th of runner function
                        // so use this pointer instead of shared_from_this()
                        [this](DeferredCallback&& n) { this->enqueue_callback(std::move(n)); },
                        [th = shared_from_this(), fn](Socket&& s, auto&& context, NotifyResult&& w) mutable {
                            th->count.waiting_async_write--;
                            auto result = w.write_unwrap(context.get_buffer(), [&](view::rvec buf) {
                                return s.write(buf);
                            });
                            if (!result) {
                                if (th->log_evt) {
                                    auto addr = s.get_remote_addr();
                                    th->log_evt(log_level::warn, addr.value_ptr(), result.error());
                                }
                            }
                            view::rvec remain;
                            if (result) {
                                remain = *result;
                            }
                            fn(std::move(s), std::move(context), remain, {th}, (bool)result.error_ptr());
                        });
                }

                bool write_async(Socket& sock, auto&& fn, auto&& context) {
                    auto s = sock.clone();
                    auto result = s.write_async_deferred(async_write_callback(std::forward<decltype(fn)>(fn), std::forward<decltype(context)>(context)));
                    if (!result) {
                        count.total_failed_write_async++;
                        auto addr = s.get_remote_addr();
                        log(log_level::warn, addr.value_ptr(), result.error());
                        return false;
                    }
                    count.total_async_write_invocation++;
                    auto concur = ++count.waiting_async_write - 1;
                    count.max_concurrent_async_write_invocation.compare_exchange_strong(concur, concur + 1);
                    return true;
                }

                void enqueue_callback(DeferredCallback&& w) {
                    count.total_queued++;
                    count.current_enqueued++;
                    io_notify << Queued{std::move(w)};
                }
            };

            struct StateContext {
               private:
                std::shared_ptr<State> s;
                friend struct State;
                friend struct MakeStateContext;  // for http3 handler
                StateContext(const std::shared_ptr<State>& s)
                    : s(s) {}

               public:
                bool read_async(Socket& sock, auto&& context, auto fn) {
                    return s->read_async(sock, std::forward<decltype(fn)>(fn),
                                         std::forward<decltype(context)>(context));
                }

                bool write_async(Socket& sock, auto&& context, auto fn) {
                    return s->write_async(sock, std::forward<decltype(fn)>(fn),
                                          std::forward<decltype(context)>(context));
                }

                void log(log_level level, NetAddrPort* addr, auto&& err) {
                    auto l = s->log_evt;
                    if (l) {
                        auto err2 = error::Error(std::forward<decltype(err)>(err));
                        l(level, addr, err2);
                    }
                }

                void log(log_level level, const char* msg, NetAddrPort& addr) {
                    log(level, &addr, error::Error(msg, error::Category::app));
                }

               private:
                template <class... Arg>
                struct InfoLog {
                    std::tuple<Arg...> tup;
                    void error(auto&& e) {
                        std::apply(
                            [&](auto&&... arg) {
                                auto print = [&](auto& arg) {
                                    if constexpr (futils::error::internal::has_error<decltype(arg)>) {
                                        arg.error(e);
                                    }
                                    else {
                                        strutil::append(e, arg);
                                    }
                                };
                                (print(arg), ...);
                            },
                            tup);
                    }

                    error::Category category() {
                        return error::Category::app;
                    }
                };

               public:
                void log(log_level level, NetAddrPort& addr, auto&&... msg) {
                    log(level, &addr, error::Error(InfoLog<decltype(msg)...>{std::forward_as_tuple(std::forward<decltype(msg)>(msg)...)}, error::Category::app));
                }

                void enqueue_callback(DeferredCallback&& w) {
                    s->enqueue_callback(std::move(w));
                }

                const Counter& state() const {
                    return s->state();
                }
            };

            inline auto make_state(void* c, ServerEntry entry) {
                return std::make_shared<State>(c, entry);
            }

        }  // namespace server
    }  // namespace fnet
}  // namespace futils

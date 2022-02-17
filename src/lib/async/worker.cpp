/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#ifdef _WIN32
#include <Windows.h>
#endif
#include "../../include/platform/windows/dllexport_source.h"

#include "../../include/async/worker.h"
#include "../../include/wrap/lite/lite.h"
#include "../../include/thread/channel.h"
#include "../../include/helper/iface_cast.h"
#include <exception>
#include <mutex>

#include <thread>

namespace utils {
    namespace async {
        using Atask = Task<Context>;
        namespace internal {
            struct TaskData {
                Atask task;
                Any* placedata = nullptr;
                size_t sigid = 0;
                TaskState state;
                void* fiber;
                Any result;
                std::exception_ptr except;

                ~TaskData() {
                    if (fiber) {
                        DeleteFiber(fiber);
                    }
                }
            };

            struct WorkerData {
                thread::SendChan<Any> w;
                thread::RecvChan<Any> r;
                wrap::hash_map<size_t, Any> wait_signal;
                thread::LiteLock lock_;
                std::atomic_size_t sigidcount = 0;
                std::atomic_uint32_t accepting = 0;
                std::atomic_size_t detached = 0;
                std::atomic_size_t maxthread = 0;
                std::atomic_size_t pooling_task = 0;
                std::atomic_bool do_yield = false;
                std::atomic_bool diepool = false;
            };

            struct ContextData {
                void* rootfiber;
                TaskData task;
                wrap::shared_ptr<WorkerData> work;
                thread::LiteLock ctxlock_;
                Context* ptr = nullptr;
                std::atomic_flag waiter_flag;
                std::atomic<ExternalTask*> outer = nullptr;
            };

            struct ContextHandle {
                static wrap::shared_ptr<ContextData>& get(Context& ctx) {
                    return ctx.data;
                }
            };

            struct ThreadToFiber {
                void* rootfiber;

                ThreadToFiber() {
                    rootfiber = ConvertThreadToFiber(nullptr);
                }

                ~ThreadToFiber() {
                    ConvertFiberToThread();
                }
            };

        }  // namespace internal

        struct CancelExcept {
        };

        struct TerminateExcept {
        };

        struct Signal {
            size_t sig;
        };

        struct SignalBack {
            std::atomic_flag sig;
            wrap::shared_ptr<internal::ContextData> data;
            Atask task;
        };

        struct EndTask {
        };

        void check_term(auto& data) {
            if (data->task.state == TaskState::term) {
                throw TerminateExcept{};
            }
            if (data->work->diepool) {
                data->task.state = TaskState::term;
                throw TerminateExcept{};
            }
        }

        void append_to_wait(internal::ContextData* c) {
            check_term(c);
            c->task.state = TaskState::wait_signal;
            c->task.sigid = ++c->work->sigidcount;
            {
                std::scoped_lock _{c->work->lock_};
                if (c->work->diepool) {
                    check_term(c);
                }
                c->work->wait_signal.emplace(c->task.sigid, std::move(*c->task.placedata));
            }
        }

        void context_switch(auto& data) {
            SwitchToFiber(data->rootfiber);
            check_term(data);
        }

        void AnyFuture::wait_or_suspend(Context& ctx) {
            check_term(data);
            if (!not_own && !is_done()) {
                auto c = internal::ContextHandle::get(ctx);
                {
                    std::scoped_lock _{data->ctxlock_};
                    data->ctxlock_.lock();
                    if (is_done()) {
                        return;
                    }
                    data->ptr = &ctx;
                    append_to_wait(c.get());
                }
                context_switch(c);
                c->task.state = TaskState::running;
            }
        }

        ExternalTask* AnyFuture::get_taskrequest() {
            if (!data) {
                return nullptr;
            }
            data->outer.wait(nullptr);
            return data->outer.load();
        }

        void AnyFuture::wait() {
            if (!data || not_own) return;
            data->waiter_flag.wait(true);
        }

        Any AnyFuture::get() {
            if (!data || not_own) return nullptr;
            data->waiter_flag.wait(true);
            return std::move(data->task.result);
        }

        TaskState AnyFuture::state() const {
            if (!data) return TaskState::invalid;
            return data->task.state;
        }

        DefferCancel::~DefferCancel() {
            if (ptr) {
                ptr->set_signal();
            }
        }

        void Context::suspend() {
            data->task.state = TaskState::suspend;
            context_switch(data);
            data->task.state = TaskState::running;
        }

        bool Context::task_wait(Task<Context>&& task) {
            if (!task) {
                return false;
            }
            append_to_wait(data.get());
            data->work->w << std::move(task);
            context_switch(data);
            data->task.state = TaskState::running;
            return true;
        }

        void Context::externaltask_wait(Any param) {
            ExternalTask task;
            task.ptr = this;
            task.param = std::move(param);
            append_to_wait(data.get());
            data->outer = &task;
            data->outer.notify_all();
            context_switch(data);
            data->outer = nullptr;
            data->task.state = TaskState::running;
        }

        void Context::set_signal() {
            if (!data->task.sigid) {
                return;
            }
            data->work->w << Signal{data->task.sigid};
            data->task.sigid = 0;
        }

        void Context::cancel() {
            check_term(data);
            throw CancelExcept{};
        }

        void Context::set_value(Any any) {
            check_term(data);
            data->task.result = std::move(any);
        }

        Any& Context::value() {
            check_term(data);
            return data->task.result;
        }

        AnyFuture Context::clone() const {
            if (!data) return {};
            check_term(data);
            AnyFuture f;
            f.data = data;
            f.not_own = true;
            return f;
        }

        void DoTask(void* pdata) {
            Context* ctx = static_cast<Context*>(pdata);
            auto data = internal::ContextHandle::get(*ctx).get();
            if (data->task.state == TaskState::term) {
                context_switch(data);
                std::terminate();  // unreachable
            }
            data->task.state = TaskState::running;
            try {
                data->task.task(*ctx);
                data->task.state = TaskState::done;
            } catch (CancelExcept& cancel) {
                data->task.state = TaskState::canceled;
            } catch (TerminateExcept& term) {
            } catch (...) {
                data->task.except = std::current_exception();
                data->task.state = TaskState::except;
            }
            context_switch(data);
            std::terminate();  // unreachable
        }

        auto& make_context(Any& holder, Context*& ctx) {
            holder = Context{};
            ctx = holder.type_assert<Context>();
            auto& data = internal::ContextHandle::get(*ctx);
            data = wrap::make_shared<internal::ContextData>();
            return data;
        }

        constexpr size_t initial_stack_size = 1024 * 2;

        auto& make_fiber(Any& place, Atask&& post, wrap::shared_ptr<internal::WorkerData> work) {
            Context* ctx;
            auto& c = make_context(place, ctx);
            c->task.task = std::move(post);
            c->task.fiber = CreateFiber(initial_stack_size, DoTask, ctx);
            c->task.state = TaskState::prelaunch;
            work->pooling_task++;
            c->work = std::move(work);
            return c;
        }

        void delete_context(auto& c) {
            DeleteFiber(c->task.fiber);
            c->task.fiber = nullptr;
        }

        void task_handler(wrap::shared_ptr<internal::WorkerData> wd) {
            internal::ThreadToFiber self;
            auto r = wd->r;
            auto w = wd->w;
            r.set_blocking(true);
            Any event;
            auto handle_fiber = [&](Any& place, internal::ContextData* c) {
                c->rootfiber = self.rootfiber;
                c->task.placedata = &place;
                if (wd->diepool) {
                    c->task.state = TaskState::term;
                }
                wd->accepting--;
                SwitchToFiber(c->task.fiber);
                wd->accepting++;
                c->task.placedata = nullptr;
                c->rootfiber = nullptr;
                if (c->task.state == TaskState::done ||
                    c->task.state == TaskState::except ||
                    c->task.state == TaskState::canceled) {
                    {
                        std::scoped_lock _{c->ctxlock_};
                        if (c->ptr) {
                            DefferCancel _{c->ptr};
                            c->ptr = nullptr;
                        }
                    }
                    delete_context(c);
                    c->work->pooling_task--;
                    c->waiter_flag.clear();
                    c->waiter_flag.notify_all();
                }
                else if (c->task.state == TaskState::term) {
                    // no signal is handled
                    delete_context(c);
                    c->work->pooling_task--;
                    if (c->work->pooling_task == 0) {
                        c->work->r.close();
                    }
                }
                else if (c->task.state == TaskState::suspend) {
                    w << std::move(place);
                }
                else if (c->task.state == TaskState::wait_signal) {
                    // nothing to do
                }
            };
            wd->accepting++;
            while (r >> event) {
                if (auto ctx = event.type_assert<Context>()) {
                    auto c = internal::ContextHandle::get(*ctx).get();
                    handle_fiber(event, c);
                }
                else {
                    if (wd->diepool) {
                        decltype(wd->wait_signal) tmp;
                        wd->lock_.lock();
                        if (wd->wait_signal.size()) {
                            tmp = std::move(wd->wait_signal);
                        }
                        wd->lock_.unlock();
                        if (tmp.size()) {
                            for (auto&& w : std::move(tmp)) {
                                auto ctx = w.second.type_assert<Context>();
                                if (ctx) {
                                    handle_fiber(w.second, internal::ContextHandle::get(*ctx).get());
                                }
                            }
                            tmp.clear();
                        }
                        continue;
                    }
                    if (auto post = event.type_assert<Atask>()) {
                        Any place;
                        auto& c = make_fiber(place, std::move(*post), wd);
                        handle_fiber(place, c.get());
                    }
                    else if (auto sigback = event.type_assert<SignalBack*>()) {
                        Any place;
                        auto p = *sigback;
                        auto& c = make_fiber(place, std::move(p->task), wd);
                        c->waiter_flag.test_and_set();
                        p->data = c;
                        p->sig.clear();
                        p->sig.notify_all();
                        handle_fiber(place, c.get());
                    }
                    else if (auto signal = event.type_assert<Signal>()) {
                        Any sig;
                        wd->lock_.lock();
                        auto found = wd->wait_signal.find(signal->sig);
                        if (found != wd->wait_signal.end()) {
                            sig = std::move(found->second);
                            wd->wait_signal.erase(signal->sig);
                        }
                        wd->lock_.unlock();
                        auto ctx = sig.type_assert<Context>();
                        if (ctx) {
                            handle_fiber(sig, internal::ContextHandle::get(*ctx).get());
                        }
                    }
                    else if (auto _ = event.type_assert<EndTask>()) {
                        if (r.peek_queue() == 0) {
                            break;
                        }
                    }
                }
                if (wd->do_yield) {
                    std::this_thread::yield();
                }
            }
            wd->accepting--;
            wd->detached--;
        }

        void TaskPool::init_data() {
            if (!data) {
                data = wrap::make_shared<internal::WorkerData>();
                auto [w, r] = thread::make_chan<Any>();
                data->w = w;
                data->r = r;
                data->maxthread = std::thread::hardware_concurrency() / 2;
            }
        }

        void TaskPool::init_thread() {
            if (data->accepting == 0) {
                if (data->detached < data->maxthread) {
                    std::thread(task_handler, data).detach();
                    data->detached++;
                }
            }
        }

        void TaskPool::init() {
            initlock.lock();
            init_data();
            init_thread();
            initlock.unlock();
        }

        void TaskPool::set_maxthread(size_t sz) {
            initlock.lock();
            init_data();
            data->maxthread = sz;
            init_thread();
            initlock.unlock();
        }

        void TaskPool::set_yield(bool do_flag) {
            initlock.lock();
            init_data();
            data->do_yield = do_flag;
            initlock.unlock();
        }

        size_t TaskPool::reduce_thread() {
            initlock.lock();
            init_data();
            initlock.unlock();
            auto r = data->accepting.load();
            if (r) {
                data->w << EndTask{};
                return 1;
            }
            return 0;
        }

        void TaskPool::posting(Task<Context>&& task) {
            init();
            data->w << std::move(task);
        }

        AnyFuture TaskPool::starting(Task<Context>&& task) {
            init();
            SignalBack back;
            back.task = std::move(task);
            back.sig.test_and_set();
            data->w << &back;
            back.sig.wait(true);
            AnyFuture f;
            f.data = back.data;
            return f;
        }

        TaskPool::~TaskPool() {
            initlock.lock();
            if (data) {
                data->lock_.lock();
                data->diepool = true;
                data->lock_.unlock();
                if (!data->pooling_task) {
                    data->r.close();
                }
            }
            initlock.unlock();
        }
    }  // namespace async
}  // namespace utils

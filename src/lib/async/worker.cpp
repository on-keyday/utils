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
                wrap::map<size_t, Any> wait_signal;
                thread::LiteLock lock_;
                std::atomic_size_t sigidcount = 0;
                std::atomic_uint32_t accepting = 0;
                std::atomic_bool diepool = false;
                std::atomic_size_t detached = 0;
                std::atomic_size_t totaldetached = 0;
                std::atomic_size_t maxthread = 0;
                std::atomic_bool do_yield = false;
            };

            struct ContextData {
                void* rootfiber;
                TaskData task;
                wrap::shared_ptr<WorkerData> work;
                thread::LiteLock ctxlock_;
                Context* ptr = nullptr;
                std::atomic_flag waiter_flag;
                std::atomic<OuterTask*> outer = nullptr;
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

        struct Signal {
            size_t sig;
            wrap::shared_ptr<internal::WorkerData> work;
        };

        struct SignalBack {
            std::atomic_flag sig;
            wrap::shared_ptr<internal::ContextData> data;
            Atask task;
        };

        struct EndTask {
        };

        void append_to_wait(internal::ContextData* c) {
            c->task.state = TaskState::wait_signal;
            c->task.sigid = ++c->work->sigidcount;
            c->work->lock_.lock();
            c->work->wait_signal.emplace(c->task.sigid, std::move(*c->task.placedata));
            c->work->lock_.unlock();
        }

        void AnyFuture::wait_or_suspend(Context& ctx) {
            if (!is_done()) {
                auto c = internal::ContextHandle::get(ctx);
                data->ctxlock_.lock();
                if (is_done()) {
                    data->ctxlock_.unlock();
                    return;
                }
                data->ptr = &ctx;
                append_to_wait(c.get());
                data->ctxlock_.unlock();
                SwitchToFiber(c->rootfiber);
                c->task.state = TaskState::running;
            }
        }

        OuterTask* AnyFuture::get_outertask() {
            if (!data) {
                return nullptr;
            }
            data->outer.wait(nullptr);
            return data->outer.load();
        }

        void AnyFuture::wait() {
            if (!data) return;
            data->waiter_flag.wait(true);
        }

        Any AnyFuture::get() {
            if (!data) return nullptr;
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
            SwitchToFiber(data->rootfiber);
            data->task.state = TaskState::running;
        }

        bool Context::wait_task(Task<Context>&& task) {
            if (!task) {
                return false;
            }
            append_to_wait(data.get());
            data->work->w << std::move(task);
            SwitchToFiber(data->rootfiber);
            data->task.state = TaskState::running;
            return true;
        }

        void Context::wait_outertask(void* param) {
            OuterTask task;
            task.ptr = this;
            task.param = param;
            append_to_wait(data.get());
            data->outer = &task;
            data->outer.notify_all();
            SwitchToFiber(data->rootfiber);
            data->outer = nullptr;
            data->task.state = TaskState::running;
        }

        void Context::set_signal() {
            if (!data->task.sigid) {
                return;
            }
            data->work->w << Signal{data->task.sigid, data->work};
            data->task.sigid = 0;
        }

        void Context::cancel() {
            throw CancelExcept{};
        }

        void Context::set_value(Any any) {
            data->task.result = std::move(any);
        }

        Any& Context::value() {
            return data->task.result;
        }

        AnyFuture Context::start_task(Task<Context>&& task) {
            auto v = SignalBack{};
            v.task = std::move(task);
            v.sig.test_and_set();
            data->work->w << &v;
            v.sig.wait(true);
            AnyFuture f;
            f.data = v.data;
            return f;
        }

        void DoTask(void* pdata) {
            Context* ctx = static_cast<Context*>(pdata);
            auto data = internal::ContextHandle::get(*ctx).get();
            data->task.state = TaskState::running;
            try {
                data->task.task(*ctx);
                data->task.state = TaskState::done;
            } catch (CancelExcept& cancel) {
                data->task.state = TaskState::canceled;
            } catch (...) {
                data->task.except = std::current_exception();
                data->task.state = TaskState::except;
            }
            SwitchToFiber(data->rootfiber);
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
            c->work = std::move(work);
            return c;
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
                wd->accepting--;
                SwitchToFiber(c->task.fiber);
                wd->accepting++;
                c->task.placedata = nullptr;
                c->rootfiber = nullptr;
                if (c->task.state == TaskState::done ||
                    c->task.state == TaskState::except ||
                    c->task.state == TaskState::canceled) {
                    c->ctxlock_.lock();
                    if (c->ptr) {
                        DefferCancel _{c->ptr};
                        c->ptr = nullptr;
                    }
                    c->ctxlock_.unlock();
                    DeleteFiber(c->task.fiber);
                    c->task.fiber = nullptr;
                    c->waiter_flag.clear();
                    c->waiter_flag.notify_all();
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
                if (auto post = event.type_assert<Atask>()) {
                    Any place;
                    auto& c = make_fiber(place, std::move(*post), wd);
                    handle_fiber(place, c.get());
                }
                else if (auto ctx = event.type_assert<Context>()) {
                    auto c = internal::ContextHandle::get(*ctx).get();
                    handle_fiber(event, c);
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
                    signal->work->lock_.lock();
                    auto found = signal->work->wait_signal.find(signal->sig);
                    if (found != signal->work->wait_signal.end()) {
                        sig = std::move(found->second);
                        signal->work->wait_signal.erase(signal->sig);
                    }
                    signal->work->lock_.unlock();
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

        void TaskPool::init() {
            initlock.lock();
            init_data();
            if (data->accepting == 0) {
                if (data->detached < data->maxthread) {
                    std::thread(task_handler, data).detach();
                    data->detached++;
                    data->totaldetached++;
                }
            }
            initlock.unlock();
        }

        void TaskPool::set_maxthread(size_t sz) {
            initlock.lock();
            init_data();
            data->maxthread = sz;
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
                data->r.close();
                data->diepool = true;
            }
            initlock.unlock();
        }
    }  // namespace async
}  // namespace utils

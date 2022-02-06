/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/async/worker.h"
#include "../../include/wrap/lite/lite.h"
#include "../../include/thread/channel.h"
#include "../../include/helper/iface_cast.h"
#include <exception>

#ifdef _WIN32
#include <Windows.h>
#endif
#include <thread>

namespace utils {
    namespace async {
        using Atask = Task<Context>;
        namespace internal {
            struct TaskData {
                Atask task;
                Atask reserved;
                size_t sigid = 0;
                TaskState state;
                void* fiber;
                Any result;
                std::exception_ptr except;
            };

            struct WorkerData {
                thread::SendChan<Any> w;
                thread::RecvChan<Any> r;
                wrap::map<size_t, Any> wait_signal;
                thread::LiteLock lock_;
                std::atomic_size_t sigidcount = 0;
            };

            struct ContextData {
                void* rootfiber;
                TaskData task;
                wrap::shared_ptr<WorkerData> work;
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

        struct Signal {
            size_t sig;
            wrap::shared_ptr<internal::WorkerData> work;
        };

        void Context::suspend() {
            data->task.state = TaskState::suspend;
            SwitchToFiber(data->rootfiber);
            data->task.state = TaskState::running;
        }

        bool Context::wait_task(Task<Context>&& task) {
            if (!task) {
                return false;
            }
            data->task.reserved = std::move(task);
            data->task.state = TaskState::wait_signal;
            data->task.sigid = ++data->work->sigidcount;
            SwitchToFiber(data->rootfiber);
            data->task.state = data->task.state = TaskState::running;
            return true;
        }

        void Context::set_signal() {
            data->work->w << Signal{data->task.sigid, data->work};
        }

        void Context::cancel() {
            data->task.state = TaskState::cahceled;
            SwitchToFiber(data->rootfiber);
        }

        void DoTask(void* pdata) {
            Context* ctx = static_cast<Context*>(pdata);
            auto data = internal::ContextHandle::get(*ctx).get();
            data->task.state = TaskState::running;
            try {
                data->task.task(*ctx);
                data->task.state = TaskState::done;
            } catch (...) {
                data->task.except = std::current_exception();
                data->task.state = TaskState::except;
            }
            SwitchToFiber(data->rootfiber);
        }

        internal::ContextData* make_context(Any& holder, Context*& ctx) {
            holder = Context{};
            ctx = holder.type_assert<Context>();
            auto& data = internal::ContextHandle::get(*ctx);
            data = wrap::make_shared<internal::ContextData>();
            return data.get();
        }

        internal::ContextData* make_fiber(Any& place, Atask&& post, wrap::shared_ptr<internal::WorkerData> work) {
            Context* ctx;
            auto c = make_context(place, ctx);
            c->task.task = std::move(post);
            c->task.fiber = CreateFiber(0, DoTask, ctx);
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
                SwitchToFiber(c->task.fiber);
                if (c->task.state == TaskState::done ||
                    c->task.state == TaskState::except ||
                    c->task.state == TaskState::cahceled) {
                    DeleteFiber(c->task.fiber);
                    c->task.fiber = nullptr;
                }
                else if (c->task.state == TaskState::suspend) {
                    w << std::move(place);
                }
                else if (c->task.state == TaskState::wait_signal) {
                    Atask wait = std::move(c->task.reserved);
                    c->work->lock_.lock();
                    c->work->wait_signal.emplace(c->task.sigid, std::move(place));
                    c->work->lock_.unlock();
                    w << std::move(wait);
                }
            };
            while (r >> event) {
                if (auto post = event.type_assert<Atask>()) {
                    Any place;
                    auto c = make_fiber(place, std::move(*post), wd);
                    handle_fiber(place, c);
                }
                else if (auto ctx = event.type_assert<Context>()) {
                    auto c = internal::ContextHandle::get(*ctx).get();
                    handle_fiber(event, c);
                }
                else if (auto signal = event.type_assert<Signal>()) {
                    Any sig;
                    signal->work->lock_.lock();
                    auto found = signal->work->wait_signal.find(signal->sig);
                    if (found != signal->work->wait_signal.end()) {
                        sig = std::move(found->second);
                    }
                    signal->work->lock_.unlock();
                    auto ctx = sig.type_assert<Context>();
                    if (ctx) {
                        handle_fiber(sig, internal::ContextHandle::get(*ctx).get());
                    }
                }
            }
        }

        void TaskPool::posting(Task<Context>&& task) {
            initlock.lock();
            if (!data) {
                data = wrap::make_shared<internal::WorkerData>();
                auto [w, r] = thread::make_chan<Any>();
                data->w = w;
                data->r = r;
                for (std::uint32_t i = 0; i < std::thread::hardware_concurrency() / 2; i++) {
                    std::thread(task_handler, data).detach();
                }
            }
            initlock.unlock();
            data->w << std::move(task);
        }

    }  // namespace async
}  // namespace utils

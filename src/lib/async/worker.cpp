/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#ifdef _WIN32
#include <Windows.h>
#else
#include <ucontext.h>
#include <cassert>
#include <unistd.h>
#include <sys/mman.h>
#endif
#include "../../include/platform/windows/dllexport_source.h"

#include "../../include/async/worker.h"
#include "../../include/wrap/lite/lite.h"
#include "../../include/thread/channel.h"
#include "../../include/thread/recursive_lock.h"
#include "../../include/helper/iface_cast.h"
#include "../../include/thread/priority_ext.h"
#include <exception>
#include <mutex>
#include <thread>

namespace utils {
    namespace async {
        using Atask = Task<Context>;

        void DoTask(void*);
        void check_term(auto& data);
#ifdef _WIN32
        using native_t = void*;
        // platform dependency
        void context_switch(native_t handle, native_t, Context*) {
            SwitchToFiber(handle);
        }

        void context_switch(auto& data, Context*) {
            context_switch(data->roothandle, nullptr, nullptr);
            check_term(data);
        }

        constexpr size_t initial_stack_size = 1024 * 64;
        constexpr size_t reserved_stack_size = initial_stack_size * 2;

        void delete_context(native_t& c) {
            DeleteFiber(c);
            c = nullptr;
        }

        native_t create_native_context(Context* ctx) {
            return CreateFiberEx(initial_stack_size, reserved_stack_size, 0, DoTask, ctx);
        }

        struct ThreadToFiber {
            native_t roothandle;

            ThreadToFiber() {
                roothandle = ConvertThreadToFiber(nullptr);
            }

            ~ThreadToFiber() {
                ConvertFiberToThread();
            }
        };

        template <class F>
        bool wait_on_context(F& future) {
            if (::IsThreadAFiber()) {
                auto data = ::GetFiberData();
                if (data) {
                    auto ctx = static_cast<Context*>(data);
                    future.wait_until(*ctx);
                    return true;
                }
            }
            return false;
        }

#else

        const std::size_t page_size_ = ::getpagesize();

        struct native_context;

        struct native_stack {
           private:
            void* map_root = nullptr;
            size_t size_ = 0;
            bool on_stack = false;
            friend native_context* move_to_stack(native_stack& stack);
            friend bool move_from_stack(native_context* ctx, native_stack& stack);

           public:
            constexpr void* root_ptr() const {
                return map_root;
            }

            void* stack_ptr() const {
                return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(map_root) + size_);
            }

            void* storage_ptr() const {
                struct nst {
                    void* a;
                    size_t b;
                    bool c;
                };
                struct nct {
                    nst n;
                    ucontext_t c;
                };
                auto p = reinterpret_cast<std::uintptr_t>(stack_ptr()) - reinterpret_cast<std::uintptr_t>(sizeof(nct));
                p &= ~static_cast<std::uintptr_t>(0xff);
                return reinterpret_cast<void*>(p);
            }

            void* func_stack_root() const {
                auto p = reinterpret_cast<std::uintptr_t>(storage_ptr()) - 64;
                return reinterpret_cast<void*>(p);
            }

            bool init(size_t times = 2) {
                if (map_root) {
                    return false;
                }
                size_ = page_size_ * (times + 1);
                auto p = ::mmap(nullptr, size_,
                                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN, -1, 0);
                if (!p || p == MAP_FAILED) {
                    return false;
                }
                ::mprotect(p, page_size_, PROT_NONE);
                map_root = p;
                return true;
            }

            bool clean() {
                if (on_stack) {
                    return false;
                }
                if (map_root) {
                    ::munmap(map_root, size_);
                    map_root = nullptr;
                    size_ = 0;
                }
                return true;
            }

            bool is_movable() const {
                return !on_stack && map_root;
            }

            size_t size() const {
                return size_;
            }

            size_t func_stack_size() const {
                auto res = reinterpret_cast<std::uintptr_t>(func_stack_root()) - reinterpret_cast<std::uintptr_t>(root_ptr());
                return res;
            }
        };

        struct native_context {
            native_stack stack;
            ucontext_t ctx;
        };

        native_context* move_to_stack(native_stack& stack) {
            if (!stack.is_movable()) {
                return nullptr;
            }
            auto storage = stack.storage_ptr();
            auto ret = new (storage) native_context();
            ret->stack.on_stack = true;
            ret->stack.map_root = stack.map_root;
            stack.map_root = nullptr;
            ret->stack.size_ = stack.size_;
            stack.size_ = 0;
            return ret;
        }

        bool move_from_stack(native_context* ctx, native_stack& stack) {
            if (!ctx ||
                !ctx->stack.map_root || !stack.on_stack ||
                stack.map_root || stack.on_stack) {
                return false;
            }
            stack.size_ = ctx->stack.size_;
            ctx->stack.size_ = 0;
            stack.map_root = ctx->stack.map_root;
            ctx->stack.map_root = nullptr;
            stack.on_stack = false;
            ctx->~native_context();
            return true;
        }

        using native_t = native_context*;

        static Context*& current_context() {
            thread_local Context* ptr = nullptr;
            return ptr;
        }

        void context_switch(native_t to, native_t from, Context* ptr) {
            auto res = ::swapcontext(&from->ctx, &to->ctx);
            assert(res == 0);
            current_context() = ptr;
        }

        void context_switch(auto& data, Context* ptr) {
            context_switch(data->roothandle, data->task.handle, ptr);
            check_term(data);
        }

        constexpr size_t initial_stack_size = 1024 * 2;

        void delete_context(native_t& c) {
            native_stack st;
            move_from_stack(c, st);
            st.clean();
            c = nullptr;
        }

        native_t create_native_context(Context* ctx) {
            native_stack stack;
            if (!stack.init()) {
                return nullptr;
            }
            auto res = move_to_stack(stack);
            assert(res);
            if (::getcontext(&res->ctx) != 0) {
                move_from_stack(res, stack);
                stack.clean();
                return nullptr;
            }
            res->ctx.uc_stack.ss_sp = res->stack.root_ptr();
            res->ctx.uc_stack.ss_size = res->stack.func_stack_size();
            res->ctx.uc_link = nullptr;
            ::makecontext(&res->ctx, (void (*)())DoTask, 1, ctx);
            return res;
        }

        struct ThreadToFiber {
            native_context* roothandle;
            ThreadToFiber() {
                roothandle = new native_context{};
                auto res = ::getcontext(&roothandle->ctx);
                assert(res == 0);
            }

            ~ThreadToFiber() {
                delete roothandle;
            }
        };

        template <class F>
        bool wait_on_context(F& future) {
            auto ctx = current_context();
            if (ctx) {
                future.wait_until(*ctx);
            }
            return false;
        }
#endif
        constexpr auto default_priority = 0x7f;

        struct compare_type {
            bool operator()(auto& a, auto& b) {
                return a.priority() > b.priority();
            }
        };

        template <class T>
        using queue_type = thread::WithRawContainer<T, wrap::queue<T>, compare_type>;

        struct PriorityReset {
            void operator()(queue_type<Event>& que) {
                /*for (auto& v : que.container()) {
                    v.set_priority(default_priority);
                }*/
            }
        };

        namespace internal {
            struct TaskData {
                Atask task;
                Event* placedata = nullptr;
                size_t sigid = 0;
                TaskState state;
                native_t handle;
                Any result;
                std::exception_ptr except;
                size_t priority = default_priority;

                ~TaskData() {
                    if (handle) {
                        delete_context(handle);
                    }
                }
            };

            struct WorkerData {
                thread::SendChan<Event, queue_type,
                                 thread::DualModeHandler<PriorityReset>, thread::RecursiveLock>
                    w;
                thread::RecvChan<Event, queue_type,
                                 thread::DualModeHandler<PriorityReset>, thread::RecursiveLock>
                    r;
                wrap::hash_map<size_t, Event> wait_signal;
                thread::LiteLock lock_;
                std::atomic_size_t sigidcount = 0;
                std::atomic_uint32_t accepting = 0;
                std::atomic_size_t detached = 0;
                std::atomic_size_t maxthread = 0;
                std::atomic_size_t pooling_task = 0;
                std::atomic_bool do_yield = true;
                std::atomic_bool diepool = false;
            };

            struct ContextData {
                native_t roothandle;
                TaskData task;
                wrap::shared_ptr<WorkerData> work;
                thread::LiteLock ctxlock_;
                wrap::weak_ptr<Context> ptr;
                std::atomic_flag waiter_flag;
                std::atomic<ExternalTask*> outer = nullptr;
                std::atomic_bool reqcancel;
                wrap::weak_ptr<Context> self;
            };

            struct ContextHandle {
                static wrap::shared_ptr<ContextData>& get(Context& ctx) {
                    return ctx.data;
                }
            };

        }  // namespace internal

        struct CancelExcept {
        };

        struct TerminateExcept {
        };

        constexpr auto priority_signal = 0;
        constexpr auto priority_endtask = ~0;

        struct Signal {
            size_t sig;
            constexpr size_t priority() const {
                return priority_signal;
            }
            constexpr void set_priority(size_t) const {}
        };

        size_t Context::priority() const {
            return data->task.priority;
        }

        void Context::set_priority(size_t e) {
            data->task.priority = e;
        }

        struct EndTask {
            constexpr size_t priority() const {
                return priority_endtask;
            }
            constexpr void set_priority(size_t) const {}
        };

        void DoTask(void* pdata) {
            Context* ctx = static_cast<Context*>(pdata);
#ifndef _WIN32
            current_context() = ctx;
#endif
            auto data = internal::ContextHandle::get(*ctx).get();
            if (data->task.state == TaskState::term) {
                context_switch(data, nullptr);
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
            context_switch(data, nullptr);
            std::terminate();  // unreachable
        }

        void check_term(auto& data) {
            if (data->task.state == TaskState::term) {
                throw TerminateExcept{};
            }
            if (data->work->diepool) {
                data->task.state = TaskState::term;
                throw TerminateExcept{};
            }
            if (data->reqcancel) {
                throw CancelExcept{};
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

        bool AnyFuture::cancel() {
            if (!data) return false;
            data->reqcancel = true;
            return true;
        }

        AnyFuture& AnyFuture::wait_until(Context& ctx) {
            if (!data) {
                return *this;
            }
            check_term(data);
            if (!not_own && !is_done()) {
                auto c = internal::ContextHandle::get(ctx);
                {
                    std::scoped_lock _{data->ctxlock_};
                    if (is_done()) {
                        return *this;
                    }
                    data->ptr = c->self;
                    append_to_wait(c.get());
                }
                context_switch(c, &ctx);
                c->task.state = TaskState::running;
            }
            return *this;
        }

        ExternalTask* AnyFuture::get_taskrequest() {
            if (!data) {
                return nullptr;
            }
            data->outer.wait(nullptr);
            return data->outer.exchange(nullptr);
        }

        void AnyFuture::wait() {
            if (!data || not_own) return;
            if (wait_on_context(*this)) {
                return;
            }
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
            if (auto l = ptr.lock(); l) {
                l->set_signal();
            }
        }

        wrap::weak_ptr<Context> DefferCancel::get_weak(Context* ptr) {
            if (!ptr) {
                return {};
            }
            return ptr->data->self;
        }

        void Context::suspend() {
            data->task.state = TaskState::suspend;
            context_switch(data, this);
            data->task.state = TaskState::running;
        }

        auto& make_context(Event& holder, Context*& ctx) {
            auto ctxptr = wrap::make_shared<Context>();
            holder = ctxptr;
            ctx = ctxptr.get();
            auto& data = internal::ContextHandle::get(*ctx);
            data = wrap::make_shared<internal::ContextData>();
            data->self = ctxptr;
            return data;
        }

        auto& make_fiber(Event& place, Atask&& post, wrap::shared_ptr<internal::WorkerData> work) {
            Context* ctx;
            auto& c = make_context(place, ctx);
            c->task.task = std::move(post);
            c->task.handle = create_native_context(ctx);
            c->task.state = TaskState::prelaunch;
            work->pooling_task++;
            c->work = std::move(work);
            return c;
        }

        bool Context::task_wait(Task<Context>&& task) {
            if (!task) {
                return false;
            }
            append_to_wait(data.get());
            Event event;
            auto& c = make_fiber(event, std::move(task), data->work);
            c->ptr = data->self;
            data->work->w << std::move(event);
            context_switch(data, this);
            data->task.state = TaskState::running;
            return true;
        }

        void Context::externaltask_wait(Any param) {
            ExternalTask task;
            task.ptr = data->self;
            task.param = std::move(param);
            append_to_wait(data.get());
            data->outer = &task;
            data->outer.notify_all();
            context_switch(data, this);
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

        void task_handler(wrap::shared_ptr<internal::WorkerData> wd) {
            ThreadToFiber self;
            auto r = wd->r;
            auto w = wd->w;
            r.set_blocking(true);
            Event event;
            auto handle_fiber = [&](Event& place, internal::ContextData* c) {
                c->roothandle = self.roothandle;
                c->task.placedata = &place;
                if (wd->diepool) {
                    c->task.state = TaskState::term;
                }
                wd->accepting--;
                context_switch(c->task.handle, c->roothandle, nullptr);
                wd->accepting++;
                c->task.placedata = nullptr;
                c->roothandle = nullptr;
                if (c->task.state == TaskState::done ||
                    c->task.state == TaskState::except ||
                    c->task.state == TaskState::canceled) {
                    {
                        std::scoped_lock _{c->ctxlock_};
                        {
                            DefferCancel _{c->ptr};
                        }
                    }
                    delete_context(c->task.handle);
                    c->work->pooling_task--;
                    c->waiter_flag.clear();
                    c->waiter_flag.notify_all();
                }
                else if (c->task.state == TaskState::term) {
                    // no signal is handled
                    delete_context(c->task.handle);
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
                if (auto ctx = event.type_assert<wrap::shared_ptr<Context>>()) {
                    auto c = internal::ContextHandle::get(**ctx).get();
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
                                auto ctx = w.second.type_assert<wrap::shared_ptr<Context>>();
                                if (ctx) {
                                    handle_fiber(w.second, internal::ContextHandle::get(**ctx).get());
                                }
                            }
                            tmp.clear();
                        }
                        continue;
                    }
                    if (auto signal = event.type_assert<Signal>()) {
                        Event sig;
                        wd->lock_.lock();
                        auto found = wd->wait_signal.find(signal->sig);
                        if (found != wd->wait_signal.end()) {
                            sig = std::move(found->second);
                            wd->wait_signal.erase(signal->sig);
                        }
                        wd->lock_.unlock();
                        auto ctx = sig.type_assert<wrap::shared_ptr<Context>>();
                        if (ctx) {
                            handle_fiber(sig, internal::ContextHandle::get(**ctx).get());
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
                auto [w, r] = thread::make_chan<Event, queue_type, thread::DualModeHandler<PriorityReset>, thread::RecursiveLock>();
                data->w = w;
                data->r = r;
                auto conc = std::thread::hardware_concurrency() / 2;
                data->maxthread = conc ? conc : 4;
            }
        }

        void detach_thread(wrap::shared_ptr<internal::WorkerData>& data) {
            std::thread(task_handler, data).detach();
            data->detached++;
        }

        void TaskPool::init_thread() {
            if (data->accepting == 0) {
                if (data->detached < data->maxthread) {
                    detach_thread(data);
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

        void TaskPool::force_run_max_thread() {
            initlock.lock();
            init_data();
            while (data->detached < data->maxthread) {
                detach_thread(data);
            }
            initlock.unlock();
        }

        void TaskPool::set_priority_mode(bool priority_mode) {
            initlock.lock();
            init_data();
            data->w.set_handler([&](auto& h) {
                h.priority_mode = priority_mode;
            });
            initlock.unlock();
        }

        void TaskPool::posting(Task<Context>&& task) {
            init();
            Event event;
            make_fiber(event, std::move(task), data);
            data->w << std::move(event);
        }

        AnyFuture TaskPool::starting(Task<Context>&& task) {
            init();
            Event event;
            auto& c = make_fiber(event, std::move(task), data);
            c->waiter_flag.test_and_set();
            data->w << std::move(event);
            AnyFuture f;
            f.data = c;
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

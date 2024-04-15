/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <coro/coro_impl.h>
#include <coro/coro.h>
#include <windows.h>
#include <coro/coro_platform.h>

namespace futils {
    namespace coro {

        struct WinHandle : Platform {
            bool was_a_fiber = false;
            void* main_fiber;

            ~WinHandle() {
                if (!main_fiber) {
                    return;
                }
                if (!was_a_fiber) {
                    ConvertFiberToThread();
                }
            }
        };

#define as_handle(ptr) static_cast<WinHandle*>(ptr)
#define as_c(ptr) static_cast<C*>(ptr)
#define get_winhandle(c) (c->is_main() ? as_handle(c->handle) : as_handle(c->main_->handle))

        static thread_local C* running_coro = nullptr;

        C* get_current() {
            return running_coro;
        }

        void C::coro_sub(C* c) {
            c->state = CState::running;
            const auto h = helper::defer([c] {
                c->state = CState::done;
                while (true) {
                    SwitchToFiber(as_handle(c->main_->handle)->main_fiber);
                }
            });
            try {
                running_coro = c;
                c->coroutine(c, c->user);
            } catch (...) {
                as_handle(c->main_->handle)->except = std::current_exception();
            }
        }

        bool C::add_coroutine(void* usr, coro_t cr) {
            if (!cr) {
                return false;
            }
            auto h = get_winhandle(this);
            if (!h) {
                return false;
            }
            C* m = nullptr;
            if (is_main()) {
                m = this;
            }
            else {
                m = this->main_;
            }
            const auto l = h->lock();
            if (h->idle.block()) {
                return false;
            }
            auto v = h->resource.alloc(sizeof(C));
            if (!v) {
                return false;
            }
            auto c = new (v) C(m, cr, usr);
            auto d = helper::defer([&] {
                h->resource.free(c);  // no destructor because no handle
            });
            auto handle = CreateFiber(8192, LPFIBER_START_ROUTINE(coro_sub), c);
            if (!handle) {
                return false;
            }
            d.cancel();
            c->handle = handle;
            h->idle.push(std::move(c));
            return true;
        }

        WinHandle* init_handle(size_t max_running, size_t max_idle, Resource res) {
            if (!res.alloc || !res.free) {
                res.alloc = ::malloc;
                res.free = ::free;
            }
            auto v = res.alloc(sizeof(WinHandle));
            if (!v) {
                return nullptr;
            }
            auto h = new (v) WinHandle();
            h->resource = res;
            auto d = helper::defer([&] {
                h->~WinHandle();
                h->resource.free(h);
            });
            if (!h->alloc_que(max_running, max_idle)) {
                return nullptr;
            }
            if (IsThreadAFiber()) {
                h->main_fiber = GetCurrentFiber();
                h->was_a_fiber = true;
            }
            else {
                h->main_fiber = ConvertThreadToFiber(nullptr);
            }
            if (!h->main_fiber) {
                return nullptr;
            }
            d.cancel();
            return h;
        }

        bool C::construct_platform(size_t max_running, size_t max_idle, Resource res) {
            auto h = init_handle(max_running, max_idle, res);
            if (!h) {
                return false;
            }
            handle = h;
            return true;
        }

        void C::destruct_platform() {
            if (!handle) {
                return;
            }
            if (is_main()) {
                while (true) {
                    if (!run_platform()) {
                        break;
                    }
                }
                auto h = as_handle(handle);
                auto dealloc = h->resource.free;
                h->~WinHandle();
                dealloc(h);
            }
            else {
                DeleteFiber(handle);
            }
        }

        void C::suspend_platform() {
            if (is_main()) {
                return;
            }
            auto h = as_handle(main_->handle);
            if (h->current != this) {
                SwitchToFiber(h->main_fiber);
                return;
            }
            this->state = CState::suspend;
            const auto d = helper::defer([&] {
                this->state = CState::running;
                running_coro = this;
            });
            h->current = nullptr;
            h->fetch_tasks();
            auto next = h->running.pop();
            bool must_suc = h->running.push(this);
            assert(must_suc);
            h->current = next;
            running_coro = next;
            if (next == nullptr) {
                SwitchToFiber(h->main_fiber);
                return;
            }
            SwitchToFiber(next->handle);
        }

        bool C::run_platform() {
            if (!is_main()) {
                return false;
            }
            auto h = as_handle(handle);
            h->fetch_tasks();
            h->current = h->running.pop();
            if (!h->current) {
                return false;
            }
            SwitchToFiber(h->current->handle);
            if (h->current && h->current->state == CState::done) {
                h->current->~C();
                h->resource.free(h->current);
            }
            h->current = nullptr;
            running_coro = nullptr;
            if (h->except) {
                auto v = std::exchange(h->except, nullptr);
                std::rethrow_exception(v);
            }
            return true;
        }

    }  // namespace coro
}  // namespace futils

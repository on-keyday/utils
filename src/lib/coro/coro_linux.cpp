/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/detect.h>
#include <ucontext.h>
#include <coro/coro_platform.h>
#include <view/iovec.h>
#include <unistd.h>
#ifdef UTILS_PLATFORM_WASI
#define _WASI_EMULATED_MMAN
#endif
#include <sys/mman.h>

namespace utils {
    namespace coro {
        struct CallStack {
           private:
            view::wvec stack;
            size_t ctrl_block = 0;

           public:
            byte* top_ptr() {
                return stack.begin();
            }

            byte* base_ptr() {
                return stack.end();
            }

            byte* ctrl_block_ptr() {
                auto ptr = std::uintptr_t(base_ptr() - ctrl_block) & ~0xff;
                return reinterpret_cast<byte*>(ptr);
            }

            byte* stack_ptr() {
                return ctrl_block_ptr() - 64;
            }

            size_t stack_size() {
                return stack.size() - std::uintptr_t(base_ptr() - stack_ptr());
            }

            bool init(size_t stack_size, size_t ctrl) {
                auto page = size_t(::getpagesize());
                auto alloc_size = ((stack_size / page) + 1) * page;
                auto p = ::mmap(nullptr, alloc_size,
                                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN, -1, 0);
                if (!p || p == MAP_FAILED) {
                    return false;
                }
                ::mprotect(p, page, PROT_NONE);
                stack = view::wvec(static_cast<byte*>(p), alloc_size);
                ctrl_block = ctrl;
                return true;
            }

            constexpr CallStack() = default;

            constexpr CallStack& operator=(CallStack&& v) noexcept {
                if (this == &v) {
                    return *this;
                }
                this->~CallStack();
                stack = std::exchange(v.stack, view::wvec());
                ctrl_block = std::exchange(v.ctrl_block, 0);
                return *this;
            }

            ~CallStack() {
                if (!stack.null()) {
                    ::munmap(stack.data(), stack.size());
                    stack = {};
                }
            }
        };

        struct Handle {
            CallStack stack;
            ucontext_t ctx{};
        };

        Handle* setup_handle(size_t stack_size, void (*call)(C*), C* arg) {
            CallStack stack;
            if (!stack.init(stack_size, sizeof(Handle))) {
                return nullptr;
            }
            auto h = reinterpret_cast<Handle*>(stack.ctrl_block_ptr());
            h->stack = std::move(stack);
            getcontext(&h->ctx);
            h->ctx.uc_stack.ss_sp = h->stack.top_ptr();
            h->ctx.uc_stack.ss_size = h->stack.stack_size();
            h->ctx.uc_link = nullptr;
            makecontext(&h->ctx, reinterpret_cast<void (*)()>(call), 1, arg);
            return h;
        }

        void delete_handle(void* p) {
            auto h = static_cast<Handle*>(p);
            CallStack s;
            s = std::move(h->stack);
            (void)s;
            // auto destruct
        }

        struct LinuxHandle : Platform {
            ucontext_t root_ctx;
            std::mutex l;
        };

#define as_coro_handle(ptr) static_cast<coro::Handle*>(ptr)
#define as_handle(ptr) static_cast<LinuxHandle*>(ptr)
#define get_linhandle(c) (c->is_main() ? as_handle(c->handle) : as_handle(c->main_->handle))

        void C::coro_sub(C* c) {
            c->state = CState::running;
            const auto h = helper::defer([c] {
                c->state = CState::done;
                auto mh = as_handle(c->main_->handle);
                while (true) {
                    swapcontext(&as_coro_handle(c->handle)->ctx,
                                &mh->root_ctx);
                }
            });
            try {
                c->coroutine(c, c->user);
            } catch (...) {
                as_handle(c->main_->handle)->except = std::current_exception();
            }
        }

        bool C::add_coroutine(void* user, coro_t f) {
            if (!f) {
                return false;
            }
            auto h = get_linhandle(this);
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
            auto c = new (v) C(m, f, user);
            auto d = helper::defer([&] {
                h->resource.free(c);  // no destructor because no handle
            });
            auto handle = setup_handle(8192, coro_sub, c);
            if (!handle) {
                return false;
            }
            d.cancel();
            c->handle = handle;
            h->idle.push(std::move(c));
            return true;
        }

        LinuxHandle* init_handle(size_t max_running, size_t max_idle, Resource res) {
            if (!res.alloc || !res.free) {
                res.alloc = ::malloc;
                res.free = ::free;
            }
            auto v = res.alloc(sizeof(LinuxHandle));
            if (!v) {
                return nullptr;
            }
            auto h = new (v) LinuxHandle();
            h->resource = res;
            auto d = helper::defer([&] {
                h->~LinuxHandle();
                h->resource.free(h);
            });
            if (!h->alloc_que(max_running, max_idle)) {
                return nullptr;
            }
            getcontext(&h->root_ctx);
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

        void C::suspend_platform() {
            if (is_main()) {
                return;
            }
            auto h = as_handle(main_->handle);
            if (h->current != this) {
                swapcontext(
                    &as_coro_handle(this->handle)->ctx,
                    &h->root_ctx);
                return;
            }
            this->state = CState::suspend;
            const auto d = helper::defer([&] {
                this->state = CState::running;
            });
            h->current = nullptr;
            h->fetch_tasks();
            auto next = h->running.pop();
            h->running.push(this);
            h->current = next;
            if (next == nullptr) {
                return;
            }
            // currently access to this is still safe
            swapcontext(&as_coro_handle(this->handle)->ctx,
                        &as_coro_handle(next->handle)->ctx);
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
                h->~LinuxHandle();
                dealloc(h);
            }
            else {
                delete_handle(handle);
            }
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
            swapcontext(&h->root_ctx,
                        &as_coro_handle(h->current->handle)->ctx);
            if (h->current && h->current->state == CState::done) {
                h->current->~C();
                h->resource.free(h->current);
            }
            h->current = nullptr;
            if (h->except) {
                auto v = std::exchange(h->except, nullptr);
                std::rethrow_exception(v);
            }
            return true;
        }

    }  // namespace coro
}  // namespace utils

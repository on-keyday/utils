/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/detect.h>
#include <low/low_dllcpp.h>
#include <low/stack.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <windows.h>
#include <winternl.h>
#endif

namespace futils::low {

#ifdef FUTILS_PLATFORM_WINDOWS
    auto set_teb(StackRange range) noexcept {
        NT_TIB* tib;
        asm volatile("mov %%gs:0x30, %0\n" : "=r"(tib));
        auto old_base = tib->StackBase;
        auto old_limit = tib->StackLimit;
        tib->StackBase = range.base;
        tib->StackLimit = range.top;
        return helper::defer([old_base, old_limit]() {
            auto tib = (NT_TIB*)NtCurrentTeb();
            auto teb = (TEB*)tib;
            tib->StackBase = old_base;
            tib->StackLimit = old_limit;
        });
    }
#endif

    void Stack::invoke_platform(void (*f)(Stack*, void*), void* c) noexcept {
        this->cb.f = f;
        this->cb.c = c;
#ifdef __x86_64__
#ifdef FUTILS_PLATFORM_WINDOWS
        auto teb = set_teb(this->range);
#endif
        // save stack pointer to this->origin
        asm volatile("mov %%rsp, %0\n" : "=r"(this->origin.rsp));
        asm volatile("mov %%rbp, %0\n" : "=r"(this->origin.rbp));
        // set stack pointer to this->top
        asm volatile(
            "mov %0, %%r8\n"
            "mov %1, %%rsp\n"  // change stack
            "mov %%rsp, %%rbp\n"
            : : "r"(this), "r"(this->range.top));
        static_assert(offsetof(Stack, resume_ptr) == 16);
#ifdef FUTILS_PLATFORM_WINDOWS
        // set r8 as arg
        // and call Stack::call
        asm volatile(
            "leaq .L.SUSPEND(%%rip), %%r9\n"
            "mov %%r9, 16(%%r8)\n"
            "mov %%r8, %%rcx\n"
            "call *%0\n"
            "ud2\n"  // this is not return normally
            ".L.SUSPEND:\n"
            : : "r"(Stack::call) : "memory");

#elif defined(FUTILS_PLATFORM_LINUX)
        // set r8 as arg
        // and call Stack::call
        asm volatile(
            "leaq .L.SUSPEND(%%rip), %%r9\n"
            "mov %%r9, 16(%%r8)\n"
            "mov %%r8, %%rdi\n"
            "callq *%0\n"
            "ud2\n"  // this is not return normally
            ".L.SUSPEND:\n"
            : : "r"(Stack::call) : "memory");
#else
#error "not implemented"
#endif
        static_assert(offsetof(Stack, origin.rsp) == 0);
        static_assert(offsetof(Stack, origin.rbp) == 8);
        // restore rbp and rsp
        asm volatile(
            "mov 0(%%rax), %%rsp\n"
            "mov 8(%%rax), %%rbp\n" : : : "rax");
#else
#error "not implemented"
#endif
    }

    void Stack::suspend_platform() noexcept {
#ifdef __x86_64__
#ifdef FUTILS_PLATFORM_WINDOWS
        auto teb = set_teb(this->range);
#endif
        // save stack pointer to this->suspended
        asm volatile(
            "mov %%rsp, %0\n"
            "mov %%rbp, %1\n"
            : "=r"(this->suspended.rsp), "=r"(this->suspended.rbp));
        // jmp to this->resume_ptr
        // get this->resume_ptr and then save RESUME address
        static_assert(offsetof(Stack, resume_ptr) == 16);
        asm volatile(
            "mov %0, %%rax\n"
            "mov 16(%%rax), %%r9\n"
            "leaq .L.RESUME(%%rip), %%r8\n"
            "mov %%r8, 16(%%rax)\n"
            "jmp *%1\n"
            ".L.RESUME:\n"
            "nop\n"
            : : "r"(this), "r"(this->resume_ptr) : "memory");
        static_assert(offsetof(Stack, suspended.rsp) == 24);
        static_assert(offsetof(Stack, suspended.rbp) == 32);
        // restore rbp and rsp
        asm volatile(
            "mov 24(%%rax), %%rsp\n"
            "mov 32(%%rax), %%rbp\n" : : : "memory");
#else
#error "not implemented"
#endif
    }

    void Stack::resume_platform() noexcept {
#ifdef __x86_64__
#ifdef FUTILS_PLATFORM_WINDOWS
        auto teb = set_teb(this->range);
#endif
        // save stack pointer to this->origin
        asm volatile(
            "mov %%rsp, %0\n"
            "mov %%rbp, %1\n"
            : "=r"(this->origin.rsp), "=r"(this->origin.rbp));
        // set stack pointer to this->suspended
        asm volatile(
            "mov %0, %%rsp\n"
            "mov %%rsp, %%rbp\n"
            : : "r"(this->suspended.rsp));
        // jmp to this->resume_ptr
        // get this->resume_ptr and then save SUSPEND address
        static_assert(offsetof(Stack, resume_ptr) == 16);
        asm volatile(
            "mov %0, %%rax\n"
            "mov 16(%%rax), %%r9\n"
            "leaq .L.SUSPEND2(%%rip), %%r8\n"
            "mov %%r8, 16(%%rax)\n"
            "jmp *%1\n"
            ".L.SUSPEND2:\n"
            "nop\n"
            : : "r"(this), "r"(this->resume_ptr));
        static_assert(offsetof(Stack, origin.rsp) == 0);
        static_assert(offsetof(Stack, origin.rbp) == 8);
        // restore rbp and rsp
        asm volatile(
            "mov 0(%%rax), %%rsp\n"
            "mov 8(%%rax), %%rbp\n" : : : "memory");
#else
#error "not implemented"
#endif
    }

    view::rvec Stack::available_stack_platform() const noexcept {
        if (!this->range.top || this->state == StackState::not_called) {
            return {};
        }
        if (this->state == StackState::on_call) {
            byte* rsp;
#ifdef __x86_64__
            asm volatile("mov %%rsp, %0\n" : "=r"(rsp));
#else
#error "not implemented"
#endif
            return {rsp, (const byte*)this->range.top};
        }
        if (this->state == StackState::suspend) {
            return {(const byte*)this->suspended.rsp, (const byte*)this->range.top};
        }
        return {};  // end_call; not available
    }
}  // namespace futils::low

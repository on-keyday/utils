/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "low_dllh.h"
#include <cstdint>
#include <core/byte.h>
#include <view/iovec.h>
#include <view/span.h>
#include <helper/defer.h>

namespace futils::low {

    enum class StackState : byte {
        not_called,
        on_call,
        suspend,
        end_call,
    };

    struct StackPtr {
        void* rsp = nullptr;
        void* rbp = nullptr;
    };

    struct Stack;

    struct StackCallback {
        void (*f)(Stack*, void*) = nullptr;
        void* c = nullptr;
    };

    struct StackRange {
        void* base = nullptr;
        void* top = nullptr;
    };

    struct low_CLASS_EXPORT Stack {
       private:
        StackPtr origin = {nullptr, nullptr};
        void* resume_ptr = nullptr;
        StackPtr suspended = {nullptr, nullptr};
        StackRange range = {nullptr, nullptr};
        StackCallback cb = {nullptr, nullptr};

        StackState state = StackState::not_called;

        void invoke_platform(void (*)(Stack*, void*), void*) noexcept;
        void suspend_platform() noexcept;
        void resume_platform() noexcept;

        view::rvec available_stack_platform() const noexcept;

        static void call(Stack* s) {
            const auto d = helper::defer([&] {
                s->state = StackState::end_call;
                s->suspend_platform();  // final suspend
                __builtin_unreachable();
            });
            s->cb.f(s, s->cb.c);
        }

       public:
        bool set(void* base, void* top) noexcept {
            if (state != StackState::not_called) {
                return false;
            }
            if (!base || !top) {
                return false;
            }
            if (std::uintptr_t(base) > std::uintptr_t(top)) {
                return false;
            }
            this->range.base = base;
            this->range.top = top;
            return true;
        }

        void invoke(void (*f)(Stack*, void*), void* c) noexcept {
            if (state != StackState::not_called || !f) {
                return;
            }
            state = StackState::on_call;
            invoke_platform(f, c);
        }

        void suspend() {
            if (state != StackState::on_call) {
                return;
            }
            state = StackState::suspend;
            suspend_platform();
            state = StackState::on_call;
        }

        void resume() {
            if (state != StackState::suspend) {
                return;
            }
            resume_platform();
        }

        view::rvec stack() const noexcept {
            return view::rvec(static_cast<byte*>(range.base),
                              static_cast<byte*>(range.top));
        }

        view::wvec stack() noexcept {
            return view::wvec(static_cast<byte*>(range.base),
                              static_cast<byte*>(range.top));
        }

        view ::rvec available_stack() const noexcept {
            return available_stack_platform();
        }

        view::wvec available_stack() noexcept {
            auto r = available_stack_platform();
            return view::wvec(const_cast<byte*>(r.begin()), const_cast<byte*>(r.end()));
        }

        template <class T = void*>
        view::rspan<T> stack_span() const noexcept {
            auto s = stack();
            if (std::uintptr_t(s.data()) % alignof(T) != 0) {
                return view::rspan<T>();
            }
            if (s.size() % sizeof(T) != 0) {
                return view::rspan<T>();
            }
            return view::rspan<T>(static_cast<T*>(range.base),
                                  (size_t)(static_cast<T*>(range.top) - static_cast<T*>(range.base)));
        }

        template <class T = void*>
        view::wspan<T> stack_span() noexcept {
            auto s = stack();
            if (std::uintptr_t(s.data()) % alignof(T) != 0) {
                return view::wspan<T>();
            }
            if (s.size() % sizeof(T) != 0) {
                return view::wspan<T>();
            }
            return view::wspan<T>(static_cast<T*>(range.base),
                                  (size_t)(static_cast<T*>(range.top) - static_cast<T*>(range.base)));
        }

        template <class T = void*>
        view::rspan<T> available_stack_span() const noexcept {
            auto s = available_stack();
            if (std::uintptr_t(s.data()) % alignof(T) != 0) {
                return view::rspan<T>();
            }
            if (s.size() % sizeof(T) != 0) {
                return view::rspan<T>();
            }
            return view::rspan<T>(static_cast<T*>(s.begin()),
                                  (size_t)(static_cast<T*>(s.end()) - static_cast<T*>(s.begin())));
        }

        template <class T = void*>
        view::wspan<T> available_stack_span() noexcept {
            auto s = available_stack();
            if (std::uintptr_t(s.data()) % alignof(T) != 0) {
                return view::wspan<T>();
            }
            if (s.size() % sizeof(T) != 0) {
                return view::wspan<T>();
            }
            return view::wspan<T>(static_cast<T*>((void*)s.begin()),
                                  (size_t)(static_cast<T*>((void*)s.end()) - static_cast<T*>((void*)s.begin())));
        }
    };
}  // namespace futils::low

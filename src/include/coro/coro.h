/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "coro_export.h"
#include "ring.h"
#include "../helper/defer.h"
#include <cstddef>

namespace utils {
    namespace coro {

        enum class CState {
            idle,
            running,
            suspend,
            done,
        };

        struct Resource {
            void* (*alloc)(size_t) = nullptr;
            void (*free)(void*) = nullptr;
        };

        struct coro_DLL_EXPORT(C) {
            using coro_t = void (*)(C*, void* user);

           private:
            void* user = nullptr;
            coro_t coroutine = nullptr;
            CState state = CState::idle;
            C* main_ = nullptr;
            void* handle = nullptr;

            static void coro_sub(C*);

            bool construct_platform(size_t max_running, size_t max_idle, Resource res);
            void destruct_platform();
            void suspend_platform();
            bool run_platform();

            constexpr C(C * m, coro_t cr, void* us)
                : user(us), coroutine(cr), main_(m) {
            }

            bool is_main() const {
                return main_ == nullptr;
            }

            friend C make_coro(size_t max_concurrent, size_t max_idle, Resource res);

            constexpr void exchange(C & in) {
                user = std::exchange(in.user, nullptr);
                coroutine = std::exchange(in.coroutine, nullptr);
                state = std::exchange(in.state, CState::idle);
                main_ = std::exchange(in.main_, nullptr);
                handle = std::exchange(in.handle, nullptr);
            }

           public:
            constexpr C() = default;

            constexpr C(C && c) {
                exchange(c);
            }

            void* set_thread_context(void* v) {
                void* prev = nullptr;
                if (is_main()) {
                    prev = user;
                    user = v;
                }
                else {
                    prev = main_->user;
                    main_->user = v;
                }
                return prev;
            }

            void* get_thread_context() const {
                if (is_main()) {
                    return user;
                }
                else {
                    return main_->user;
                }
            }

            C& operator=(C&& in) {
                if (this == &in) {
                    return *this;
                }
                destruct_platform();
                exchange(in);
                return *this;
            }

            bool add_coroutine(void* v, coro_t c);

            template <class T>
            bool add_coroutine(T * t, void (*c)(C*, T*)) {
                return add_coroutine(static_cast<void*>(t), reinterpret_cast<coro_t>(c));
            }

            bool run() {
                if (!handle) {
                    return false;
                }
                return run_platform();
            }

            void suspend() {
                if (!handle) {
                    return;
                }
                suspend_platform();
            }

            constexpr explicit operator bool() const {
                return handle != nullptr;
            }

            ~C() {
                destruct_platform();
            }
        };

        using coro_t = C::coro_t;

        inline C make_coro(size_t max_running = 10, size_t max_idle = 10, Resource res = {}) {
            C c;
            if (!c.construct_platform(max_running, max_idle, res)) {
                return {};
            }
            return c;
        }

    }  // namespace coro
}  // namespace utils

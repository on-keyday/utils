/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// native_context - native context impl
#pragma once
#include "../../../include/async/light/context.h"
#include <cstdint>
#ifdef __linux__
#include <ucontext.h>
#include <cassert>
#include <unistd.h>
#include <sys/mman.h>

namespace utils {
    namespace async {
        namespace light {

            struct native_handle_t;
            struct native_stack {
               private:
                void* map_root = nullptr;
                size_t size_ = 0;
                bool on_stack = false;
                friend native_handle_t* move_to_stack(native_stack& stack);
                friend bool move_from_stack(native_handle_t* ctx, native_stack& stack);

               public:
                constexpr void* root_ptr() const {
                    return map_root;
                }

                void* stack_ptr() const;

                void* storage_ptr() const;

                void* func_stack_root() const;

                bool init(size_t times = 2);

                bool clean();

                constexpr bool is_movable() const {
                    return !on_stack && map_root;
                }

                constexpr size_t size() const {
                    return size_;
                }

                size_t func_stack_size() const;
            };

            ucontext_t* get_ucontext(native_handle_t* h);

            ucontext_t*& ucontext_from(native_handle_t* h);

            native_handle_t* move_to_stack(native_stack& stack);

            bool move_from_stack(native_handle_t* ctx, native_stack& stack);
#endif
        }  // namespace light
    }      // namespace async
}  // namespace utils

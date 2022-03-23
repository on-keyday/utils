/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "native_context_impl.h"
#include <new>
#ifdef __linux__
namespace utils {
    namespace async {
        namespace light {
            const std::size_t page_size_ = ::getpagesize();

            struct native_handle_t {
                native_stack stack;
                ucontext_t ctx;
                ucontext_t* from_ctx;
            };

            ucontext_t* get_ucontext(native_handle_t* h) {
                assert(h);
                return &h->ctx;
            }

            ucontext_t*& ucontext_from(native_handle_t* h) {
                assert(h);
                return h->from_ctx;
            }

            void* native_stack::stack_ptr() const {
                return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(map_root) + size_);
            }

            void* native_stack::storage_ptr() const {
                auto p = reinterpret_cast<std::uintptr_t>(stack_ptr()) - reinterpret_cast<std::uintptr_t>(sizeof(native_handle_t));
                p &= ~static_cast<std::uintptr_t>(0xff);
                return reinterpret_cast<void*>(p);
            }

            void* native_stack::func_stack_root() const {
                auto p = reinterpret_cast<std::uintptr_t>(storage_ptr()) - 64;
                return reinterpret_cast<void*>(p);
            }

            bool native_stack::init(size_t times) {
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

            bool native_stack::clean() {
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

            size_t native_stack::func_stack_size() const {
                auto res = reinterpret_cast<std::uintptr_t>(func_stack_root()) - reinterpret_cast<std::uintptr_t>(root_ptr());
                return res;
            }

            native_handle_t* move_to_stack(native_stack& stack) {
                if (!stack.is_movable()) {
                    return nullptr;
                }
                auto storage = stack.storage_ptr();
                auto ret = new (storage) native_handle_t();
                ret->stack.on_stack = true;
                ret->stack.map_root = stack.map_root;
                stack.map_root = nullptr;
                ret->stack.size_ = stack.size_;
                stack.size_ = 0;
                return ret;
            }

            bool move_from_stack(native_handle_t* ctx, native_stack& stack) {
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
                ctx->~native_handle_t();
                return true;
            }
        }  // namespace light
    }      // namespace async
}  // namespace utils
#endif

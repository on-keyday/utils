/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// shared_ptr - shared pointer
#pragma once
#include <atomic>
#include "alloc.h"

namespace utils {
    namespace quic {
        namespace mem {
            template <class T, bool atomic>
            struct Storage {
                T value;
                using uint_t = std::conditional_t<atomic, std::atomic_uint32_t, std::uint32_t>;
                uint_t weak_count;
                uint_t use_count;
                allocate::Alloc* alloc;
                T* get() const noexcept {
                    return std::addressof(const_cast<T&>(value));
                }

                template <class... Args>
                Storage(size_t v, Args&&... args)
                    : value(std::forward<Args>(args)...) {
                    weak_count = v;
                    use_count = v;
                }

                void incr_use() {
                    use_count++;
                }

                void decr_weak() noexcept {
                    if (weak_count-- == 1) {
                        alloc->deallocate(this);
                    }
                }

                void decr_use() noexcept {
                    if (use_count-- == 1) {
                        value.~T();
                        decr_weak();
                    }
                }
            };

            struct Alloc : allocate::Alloc {
                ~Alloc() {
                    if (free) {
                        free(C);
                    }
                }
            };

            template <class T, bool atomic = true>
            struct shared_ptr {
               private:
                using storage = Storage<T, atomic>;
                storage* st;
                template <class V, class... Arg>
                friend shared_ptr<V> make_shared(allocate::Alloc*, Arg&&...);
                template <class V, class... Arg>
                friend shared_ptr<V> make_inner_alloc(allocate::Alloc a, auto&& get_alloc, Arg&&...);

                shared_ptr(storage* t)
                    : st(t) {}

                void copy_storage(storage* other) {
                    if (other) {
                        other->incr_use();
                    }
                    if (st) {
                        st->decr_use();
                    }
                    st = other;
                }

                void move_storage(storage*& other) {
                    if (st) {
                        st->decr_use();
                    }
                    st = other;
                    other = nullptr;
                }

               public:
                constexpr shared_ptr()
                    : st(nullptr) {}

                constexpr shared_ptr(std::nullptr_t)
                    : st(nullptr) {}

                shared_ptr(const shared_ptr& other)
                    : st(nullptr) {
                    copy_storage(other.st);
                }

                shared_ptr(shared_ptr&& other)
                    : st(nullptr) {
                    move_storage(other.st);
                }

                shared_ptr& operator=(const shared_ptr& other) {
                    copy_storage(other.st);
                    return *this;
                }

                shared_ptr& operator=(shared_ptr&& other) {
                    move_storage(other.st);
                    return *this;
                }

                T* get() const {
                    return st ? st->get() : nullptr;
                }

                T* operator->() const {
                    return get();
                }

                T* operator*() const {
                    return get();
                }

                operator bool() {
                    return st != nullptr;
                }

                ~shared_ptr() {
                    if (st) {
                        st->decr_use();
                        st = nullptr;
                    }
                }
            };

            template <class V, class... Arg>
            shared_ptr<V> make_inner_alloc(allocate::Alloc a, auto&& get_alloc, Arg&&...) {
                auto storage = a.allocate<Storage<V, true>>(1);
                if (!storage) {
                    return nullptr;
                }
                storage->alloc = get_alloc(storage->value, a);
                if (!storage->alloc) {
                    a.deallocate(storage);
                    return nullptr;
                }
                return shared_ptr<V, true>{storage};
            }

            using shared_alloc = shared_ptr<Alloc, true>;

            template <class T, class... Args>
            shared_ptr<T, true> make_shared(allocate::Alloc* a, Args&&... args) {
                if (!a) {
                    return nullptr;
                }
                auto* storage = a->allocate<Storage<T, true>>(1, std::forward<Args>(args)...);
                if (!storage) {
                    return nullptr;
                }
                storage->alloc = a;
                return shared_ptr<T, true>{storage};
            }
        }  // namespace mem
    }      // namespace quic
}  // namespace utils

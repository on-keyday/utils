/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// shared_ptr - shared pointer
#pragma once
#include <atomic>
#include <utility>
#include "alloc.h"
#include "callback.h"

namespace utils {
    namespace quic {
        namespace mem {
            template <class T, bool atomic>
            struct Storage {
                union {
                    T value;
                    byte data[sizeof(T)];
                };
                using uint_t = std::conditional_t<atomic, std::atomic_uint32_t, std::uint32_t>;
                uint_t weak_count;
                uint_t use_count;
                allocate::Alloc* alloc;
                T* get() const noexcept {
                    return std::addressof(const_cast<T&>(value));
                }

                template <class... Args>
                Storage(size_t v, Args&&... args) {
                    weak_count = v;
                    use_count = v;
                    new (data) T(std::forward<Args>(args)...);
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

                ~Storage() {}
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
                // unsafe
                constexpr shared_ptr(storage* t)
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

                storage* get_storage(bool incr_use) {
                    if (st && incr_use) {
                        st->incr_use();
                    }
                    return st;
                }

                static shared_ptr from_raw_storage(void* raw) {
                    return shared_ptr<T, atomic>(static_cast<storage*>(raw));
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

            template <class RetTraits>
            struct StoragedRet {
                using Ret = typename RetTraits::Ret;
                allocate::Alloc* a;
                template <class Ptr>
                static constexpr auto& get_callable(void* a) noexcept {
                    return static_cast<Ptr>(a)->value.value;
                }

                template <class T>
                struct Copyable {
                    void (*copy)(void** to, void* from) = nullptr;
                    void (*destruct)(void* v) = nullptr;
                    T value;
                };

                template <class T>
                constexpr auto get_address(T&& addr) noexcept {
                    using T_ = Copyable<std::decay_t<T>>;
                    shared_ptr<T_> storage = mem::make_shared<T_>(
                        a,
                        T_{[](void** to, void* from) {
                               auto raw = shared_ptr<T_>::from_raw_storage(from);
                               *to = raw.get_storage(true);
                               raw.get_storage(true);
                           },
                           [](void* p) {
                               shared_ptr<T_>::from_raw_storage(p);
                           },
                           std::forward<T>(addr)});
                    auto st = storage.get_storage(true);
                    return st;
                }

                static constexpr void move(auto& cb, auto& from_cb, auto& func_ctx, auto& from_ctx) {
                    cb = std::exchange(from_cb, nullptr);
                    func_ctx = std::exchange(from_ctx, nullptr);
                }

                static constexpr void copy(auto& cb, auto& from_cb, void*& func_ctx, void* from_ctx) {
                    if (from_ctx) {
                        auto dummy = static_cast<Storage<Copyable<std::max_align_t>, true>*>(from_ctx);
                        dummy->value.copy(&func_ctx, from_ctx);
                    }
                    cb = from_cb;
                }

                static void destruct(auto& cb, void*& data) {
                    if (data) {
                        auto dummy = static_cast<Storage<Copyable<std::max_align_t>, true>*>(data);
                        dummy->value.destruct(data);
                    }
                    data = nullptr;
                    cb = nullptr;
                }

                static constexpr auto default_v() {
                    return RetTraits::default_v();
                }
            };

            template <class Ret, class... Args>
            using CBS = CBCore<nocontext_t, nocontext_t, StoragedRet<RetDefault<Ret>>, Args...>;

            template <class Ret, class... Args, class F>
            CBS<Ret, Args...> make_cb(allocate::Alloc* a, F&& f) {
                CBS<Ret, Args...> cb;
                cb = {{a}, std::forward<F>(f)};
                if (!cb.func_ctx) {
                    return nullptr;
                }
                return cb;
            }

        }  // namespace mem
    }      // namespace quic
}  // namespace utils

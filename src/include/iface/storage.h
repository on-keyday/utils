/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// storage - interface base storage
#pragma once
#include <memory>
#include <type_traits>
#include <cassert>
#include "macros.h"

namespace utils {
    namespace iface {

        struct Ref {
           protected:
            void* refptr = nullptr;

            constexpr Ref(void* p)
                : refptr(p) {}

           public:
            DEFAULT_METHODS(Ref)

            template <class T>
            constexpr Ref(T& t)
                : refptr(std::addressof(t)) {}

            constexpr void* ptr() const {
                return refptr;
            }

            bool operator==(std::nullptr_t) const {
                return refptr == nullptr;
            }
        };

        using deleter_t = void (*)(void*, void*);

        struct Owns : Ref {
           private:
            deleter_t deleter = nullptr;

            template <class T>
            static void* select_pointer(T ptr) {
                using T_ = std::remove_cvref_t<T>;
                return new T_{std::move(ptr)};
            }

            template <class T>
            static deleter_t select_deleter() {
                return [](void* p, void*) {
                    delete static_cast<T*>(p);
                };
            }

           public:
            constexpr Owns() = default;

            template <class T>
            constexpr Owns(const T& t)
                : deleter(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(select_pointer<const T&>(t)) {}

            template <class T, std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
            constexpr Owns(T&& t)
                : deleter(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(select_pointer<T&&>(std::forward<T>(t))) {}

            constexpr Owns(const Owns&) = delete;

            constexpr Owns(Owns&& in)
                : deleter(std::exchange(in.deleter, nullptr)),
                  Ref(std::exchange(in.refptr, nullptr)) {}

            constexpr Owns& operator=(Owns&& in) {
                this->~Owns();
                deleter = std::exchange(in.deleter, nullptr);
                refptr = std::exchange(in.refptr, nullptr);
                return *this;
            }

            constexpr ~Owns() {
                if (deleter) {
                    deleter(refptr, nullptr);
                }
            }
        };

        template <class Alloc>
        struct Aowns : Ref {
            using allocator_t = Alloc;
            using traits_t = std::allocator_traits<Alloc>;

           private:
            allocator_t alloc{};
            deleter_t deleter = nullptr;

            template <class T>
            static void* select_pointer(T t, allocator_t& alloc) {
                using T_ = std::remove_cvref_t<T>;
                auto p = traits_t::allocate(alloc, sizeof(T));
                traits_t::construct(alloc, p, std::forward<T>(t));
                return p;
            }

            template <class T>
            static deleter_t select_deleter() {
                return [](void* ptr, void* alloc) {
                    auto t = static_cast<T*>(ptr);
                    auto al = static_cast<allocator_t*>(alloc);
                    traits_t::destory(*alloc, ptr);
                    traits_t::deallocate(*alloc, ptr);
                };
            }

           public:
            constexpr Aowns() = default;

            template <class T>
            constexpr Aowns(const T& t)
                : deleter(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(select_pointer<const T&>(t, alloc)) {}

            template <class T, std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
            constexpr Aowns(T&& t)
                : deleter(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(select_pointer<T&&>(std::forward<T>(t), alloc)) {}

            constexpr Aowns(const Aowns&) = delete;

            constexpr Aowns(Aowns&& in)
                : deleter(std::exchange(in.deleter, nullptr)),
                  alloc(std::exchange(in.alloc, {})),
                  Ref(std::exchange(in.refptr, nullptr)) {}

            constexpr Aowns& operator=(Aowns&& in) {
                this->~Aowns();
                deleter = std::exchange(in.deleter, nullptr);
                refptr = std::exchange(in.refptr, nullptr);
                alloc = std::exchange(in.alloc, {});
                return *this;
            }

            constexpr ~Aowns() {
                if (deleter) {
                    deleter(refptr, &alloc);
                }
            }
        };

        struct Powns : Ref {
           private:
            static constexpr std::uintptr_t prim = 0x1;
            static constexpr std::uintptr_t mask = prim;
            std::uintptr_t types = 0;

            template <class T>
            static constexpr bool is_prim = sizeof(T) <= sizeof(void*) &&
                                            alignof(T) <= alignof(void*) &&
                                            std::is_trivial_v<T>;

            template <class T>
            static void* select_pointer(T t) {
                using T_ = std::remove_cvref_t<T>;
                if constexpr (is_prim<T_>) {
                    void* v = nullptr;
                    *reinterpret_cast<T_*>(&v) = std::forward<T>(t);
                    return v;
                }
                else {
                    return new T_{std::forward<T>(t)};
                }
            }

            template <class T>
            static std::uintptr_t select_deleter() {
                if constexpr (is_prim<T>) {
                    return prim;
                }
                else {
                    static deleter_t delptr = [](void* del, void*) {
                        delete static_cast<T*>(del);
                    };
                    std::uintptr_t unsafeptr = reinterpret_cast<std::uintptr_t>(&delptr);
                    assert((unsafeptr & mask) == 0);
                    return unsafeptr;
                }
            }

            static deleter_t get_deleter(std::uintptr_t ptr) {
                if (ptr & prim || ptr == 0) {
                    return nullptr;
                }
                ptr &= ~mask;
                deleter_t* del = reinterpret_cast<deleter_t*>(ptr);
                return *del;
            }

           public:
            constexpr Powns() {}

            template <class T>
            constexpr Powns(T& t)
                : types(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(select_pointer<T&>(t)) {}

            template <class T>
            constexpr Powns(const T& t)
                : types(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(select_pointer<const T&>(t)) {}

            template <class T, std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
            constexpr Powns(T&& t)
                : types(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(select_pointer<T&&>(std::forward<T>(t))) {}

            constexpr Powns(const Powns&) = delete;

            constexpr Powns(Powns&& in)
                : types(std::exchange(in.types, 0)),
                  Ref(std::exchange(in.refptr, nullptr)) {}

            constexpr Powns& operator=(Powns&& in) {
                this->~Powns();
                types = std::exchange(in.types, 0);
                refptr = std::exchange(in.refptr, nullptr);
                return *this;
            }

            constexpr void* ptr() const {
                if (types & prim) {
                    return reinterpret_cast<void*>(const_cast<void**>(&refptr));
                }
                else {
                    return Ref::ptr();
                }
            }

            constexpr bool operator==(std::nullptr_t) const {
                return types == 0;
            }

            ~Powns() {
                if (auto del = get_deleter(types)) {
                    del(this->ptr(), nullptr);
                }
            }
        };
    }  // namespace iface
}  // namespace utils

#include "undef_macros.h"  // clear macros

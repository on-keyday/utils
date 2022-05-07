/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// base_iface - basic interface class
#pragma once
#include <memory>

namespace utils {
    namespace iface {
        struct Ref {
           protected:
            void* refptr = nullptr;

           public:
            constexpr Ref() = default;
            constexpr Ref(const Ref&) = default;
            constexpr Ref& operator=(const Ref&) = default;
            template <class T>
            constexpr Ref(T& t)
                : refptr(std::addressof(t)) {}

            template <class T, class Fn>
            constexpr Ref(T& t, Fn&& from)
                : refptr(from(t)) {}

            constexpr Ref(void* p)
                : refptr(p) {}

            constexpr void* ptr() const {
                return refptr;
            }
        };

        template <class T>
        constexpr bool is_narrow_enough = sizeof(T) <= sizeof(void*) &&
                                          alignof(T) <= alignof(void*);

        template <class T>
        void* select_pointer(T&& ptr) {
            if constexpr (is_narrow_enough<T>) {
                void** res = static_cast<void**>(std::addressof(ptr));
                return *res;
            }
            else {
                using T_ = std::remove_cvref_t<T>;
                return new T_{std::move(ptr)};
            }
        }

        using deleter_t = void (*)(void*);

        template <class T>
        deleter_t select_deleter() {
            if constexpr (is_narrow_enough<T>) {
                return nullptr;
            }
            else {
                return [](void* p) {
                    delete static_cast<T*>(p);
                };
            }
        }

        struct Owns : Ref {
           private:
            deleter_t deleter = nullptr;

           public:
            constexpr Owns() = default;

            template <class T>
            constexpr Owns(T&& t)
                : deleter(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(t, select_pointer<T>) {}

            template <class T, class O>
            constexpr Owns(T* t, void (*del)(O*))
                : deleter(static_cast<deleter_t>(del)),
                  Ref(static_cast<void*>(t)) {}

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
                    deleter(refptr);
                }
            }
        };
#define MAKE_FN(func_name, retty, ...)                                       \
    retty (*func_name##_ptr)(void* __VA_OPT__(, ) __VA_ARGS__) = nullptr;    \
    template <class T, class... Args>                                        \
    static retty func_name##_fn(void* ptr, Args... args) {                   \
        return static_cast<T*>(ptr)->func_name(std::forward<Args>(args)...); \
    }
#define APPLY1_FN(func_name, ...) func_name##_fn<std::remove_cvref_t<T> __VA_OPT__(, ) __VA_ARGS__>
#define APPLY2_FN(func_name, ...) \
    func_name##_ptr(APPLY1_FN(func_name, __VA_ARGS__))
#define DEFAULT_CALL(func_name, defaultv, ...) return this->refptr && func_name##_ptr ? func_name##_ptr(this->refptr __VA_OPT__(, ) __VA_ARGS__) : defaultv;

        template <class Box>
        struct String : Box {
           private:
            MAKE_FN(data, char*)
            MAKE_FN(size, size_t)
           public:
            constexpr String() = default;
            template <class T>
            constexpr String(T&& t)
                : APPLY2_FN(data),
                  APPLY2_FN(size),
                  Box(t) {}

            char* data() {
                DEFAULT_CALL(data, nullptr)
            }

            const char* c_str() const {
                DEFAULT_CALL(data, nullptr)}

            size_t size() const {
                DEFAULT_CALL(size, 0)
            }
        };
    }  // namespace iface
}  // namespace utils

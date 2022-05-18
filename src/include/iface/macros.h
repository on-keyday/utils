/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// macros - interface definition helper macro
#pragma once

namespace utils {
    namespace iface {
#define DEFAULT_METHODS(Type)                         \
    constexpr Type() = default;                       \
    constexpr Type(Type&) = default;                  \
    constexpr Type(const Type&) = default;            \
    constexpr Type(Type&&) = default;                 \
    constexpr Type& operator=(const Type&) = default; \
    constexpr Type& operator=(Type&&) = default;

#define MAKE_FN(func_name, retty, ...)                                       \
    retty (*func_name##_ptr)(void* __VA_OPT__(, ) __VA_ARGS__) = nullptr;    \
    template <class T, class... Args>                                        \
    static retty func_name##_fn(void* ptr, Args... args) {                   \
        return static_cast<T*>(ptr)->func_name(std::forward<Args>(args)...); \
    }
#define MAKE_FN_VOID(func_name, ...)                                     \
    void (*func_name##_ptr)(void* __VA_OPT__(, ) __VA_ARGS__) = nullptr; \
    template <class T, class... Args>                                    \
    static void func_name##_fn(void* ptr, Args... args) {                \
        static_cast<T*>(ptr)->func_name(std::forward<Args>(args)...);    \
    }
#define APPLY1_FN(func_name, ...) func_name##_fn<std::remove_cvref_t<T> __VA_OPT__(, ) __VA_ARGS__>
#define APPLY2_FN(func_name, ...) \
    func_name##_ptr(APPLY1_FN(func_name, __VA_ARGS__))
#define DEFAULT_CALL(func_name, defaultv, ...) return this->ptr() && func_name##_ptr ? func_name##_ptr(this->ptr() __VA_OPT__(, ) __VA_ARGS__) : defaultv;

    }  // namespace iface
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// macros - interface definition helper macro
// use with undef_macros.h

namespace utils {
    namespace iface {
#define DEFAULT_METHODS(Type)                         \
    constexpr Type() = default;                       \
    constexpr Type(Type&) = default;                  \
    constexpr Type(const Type&) = default;            \
    constexpr Type(Type&&) = default;                 \
    constexpr Type& operator=(const Type&) = default; \
    constexpr Type& operator=(Type&&) = default;

#define DEFAULT_METHODS_NONCONSTEXPR(Type)  \
    Type() = default;                       \
    Type(Type&) = default;                  \
    Type(const Type&) = default;            \
    Type(Type&&) = default;                 \
    Type& operator=(const Type&) = default; \
    Type& operator=(Type&&) = default;

#define DEFAULT_METHODS_MOVE(Type, ...) \
    __VA_OPT__(constexpr)               \
    Type() = default;                   \
    __VA_OPT__(constexpr)               \
    Type(Type&&) = default;             \
    __VA_OPT__(constexpr)               \
    Type& operator=(Type&&) = default;  \
    __VA_OPT__(constexpr)               \
    Type(const Type&) = delete;         \
    __VA_OPT__(constexpr)               \
    Type& operator=(const Type&) = delete;

#define DEFAULT_METHODS_MOVE_NODEL(Type, ...) \
    __VA_OPT__(constexpr)                     \
    Type() = default;                         \
    __VA_OPT__(constexpr)                     \
    Type(Type&&) = default;                   \
    __VA_OPT__(constexpr)                     \
    Type& operator=(Type&&) = default;

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

#define APPLY2_FN(func_name, ...) \
    func_name##_ptr(func_name##_fn<std::remove_cvref_t<T> __VA_OPT__(, ) __VA_ARGS__>)
#define DEFAULT_CALL(func_name, defaultv, ...) return this->ptr() && func_name##_ptr ? func_name##_ptr(this->ptr() __VA_OPT__(, ) __VA_ARGS__) : defaultv;

#define MAKE_FN_EXISTS(func_name, rettype, cond, defaultv, ...)                      \
    rettype (*func_name##_ptr)(void* __VA_OPT__(, ) __VA_ARGS__) = nullptr;          \
    template <class T, class... Args>                                                \
    static rettype func_name##_fn(void* ptr, Args... args) {                         \
        if constexpr (cond) {                                                        \
            if constexpr (std::is_same_v<rettype, void>) {                           \
                static_cast<T*>(ptr)->func_name(std::forward<Args>(args)...);        \
            }                                                                        \
            else {                                                                   \
                return static_cast<T*>(ptr)->func_name(std::forward<Args>(args)...); \
            }                                                                        \
        }                                                                            \
        else {                                                                       \
            return defaultv;                                                         \
        }                                                                            \
    }
#define MAKE_FN_EXISTS_VOID(func_name, cond, ...) MAKE_FN_EXISTS(func_name, void, cond, (void)0 __VA_OPT__(, ) __VA_ARGS__)

#define REJECT_SELF(T, This) std::enable_if_t<!std::is_same_v<std::remove_cvref_t<T>, This>, int> = 0
    }  // namespace iface
}  // namespace utils

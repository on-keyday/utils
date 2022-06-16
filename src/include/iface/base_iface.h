/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// base_iface - basic interface class
#pragma once
#include <type_traits>
#include "macros.h"

namespace utils {
    namespace iface {

        template <class Box>
        struct Sized : Box {
           private:
            MAKE_FN(size, size_t)
           public:
            DEFAULT_METHODS(Sized)

            template <class T>
            constexpr Sized(T&& t)
                : APPLY2_FN(size), Box(std::forward<T>(t)) {}

            size_t size() const {
                DEFAULT_CALL(size, 0)
            }
        };

        template <class Box, class Char = char>
        struct String : Box {
           private:
            MAKE_FN(data, Char*)
           public:
            DEFAULT_METHODS(String)

            template <class T>
            constexpr String(T&& t)
                : APPLY2_FN(data),
                  Box(std::forward<T>(t)) {}

            Char* data() {
                DEFAULT_CALL(data, nullptr)
            }

            const Char* c_str() const {
                DEFAULT_CALL(data, nullptr)
            }
        };

        template <class Box, class Char = char>
        struct PushBacker : Box {
           private:
            MAKE_FN_VOID(push_back, Char)
           public:
            DEFAULT_METHODS(PushBacker)

            template <class T>
            constexpr PushBacker(T&& t)
                : APPLY2_FN(push_back, Char),
                  Box(std::forward<T>(t)) {}

            void push_back(Char c) {
                DEFAULT_CALL(push_back, (void)0, c)
            }
        };

        template <class Box, class Char = char>
        struct Buffer : Box {
           private:
            MAKE_FN_VOID(erase, size_t, size_t)
            MAKE_FN_VOID(pop_back)
           public:
            DEFAULT_METHODS(Buffer)

            template <class T>
            constexpr Buffer(T&& t)
                : APPLY2_FN(erase, size_t, size_t),
                  APPLY2_FN(pop_back),
                  Box(std::forward<T>(t)) {}

            void erase(size_t offset, size_t count) {
                DEFAULT_CALL(erase, (void)0, offset, count)
            }

            void pop_back() {
                DEFAULT_CALL(pop_back, (void)0)
            }
        };

        namespace internal {
            template <class T>
            concept has_copy = requires(T t) {
                {t.copy()};
            };
        }  // namespace internal

        template <class Box>
        struct Copy : Box {
           private:
            Copy (*copy_ptr)(void*) = nullptr;

            template <class T>
            static Copy copy_fn(void* p) {
                if constexpr (internal::has_copy<T>) {
                    return static_cast<T*>(p)->copy();
                }
                else {
                    return *static_cast<T*>(p);
                }
            }

           public:
            DEFAULT_METHODS_MOVE_NODEL(Copy)

            template <class T>
            Copy(T&& t)
                : APPLY2_FN(copy),
                  Box(std::forward<T>(t)) {}

            Copy(const Copy& cp)
                : Copy(cp.copy()) {}

            Copy& operator=(const Copy& cp) {
                *this = cp.copy();
            }

            Copy copy() const {
                DEFAULT_CALL(copy, Copy{})
            }
        };

        template <class Value>
        Value select_traits() {
            if constexpr (std::is_reference_v<Value>) {
                int* fatal = 0;
                *fatal = 0;
                using V = std::remove_cvref_t<Value>;
                static V v;
                return v;
            }
            else {
                return Value{};
            }
        }

        template <class Box, class Key = size_t, class Value = char>
        struct Subscript : Box {
           private:
            Value (*subscript_ptr)(void*, Key) = nullptr;

            template <class U>
            static Value subscript_fn(void* p, Key i) {
                return (*static_cast<U*>(p))[std::move(i)];
            }

           public:
            DEFAULT_METHODS(Subscript)

            template <class T>
            Subscript(T&& t)
                : APPLY2_FN(subscript),
                  Box(std::forward<T>(t)) {}

            Value operator[](Key index) const {
                DEFAULT_CALL(subscript, select_traits<Value>(), std::move(index))
            }
        };

        template <class Box, class Value>
        struct Deref : Box {
           private:
            Value (*deref_ptr)(void*) = nullptr;
            template <class T>
            static Value deref_fn(void* p) {
                return **static_cast<T*>(p);
            }

           public:
            DEFAULT_METHODS(Deref)

            template <class T>
            Deref(T&& t)
                : APPLY2_FN(deref),
                  Box(std::forward<T>(t)) {}

            Value operator*() const {
                DEFAULT_CALL(deref, select_traits<Value>())
            }
        };

        template <class Box>
        struct Increment : Box {
           private:
            void (*increment_ptr)(void*) = nullptr;
            template <class T>
            static void increment_fn(void* p) {
                ++*static_cast<T*>(p);
            }

           public:
            DEFAULT_METHODS(Increment)

            template <class T>
            Increment(T&& t)
                : APPLY2_FN(increment),
                  Box(std::forward<T>(t)) {}

            Increment& operator++() {
                increment_ptr ? increment_ptr(this->refptr) : (void)0;
                return *this;
            }

            Increment& operator++(int) {
                return ++*this;
            }
        };

        template <class Box>
        struct Compare : Box {
           private:
            bool (*compare_ptr)(void*, void*) = nullptr;

            template <class T>
            static bool compare_fn(void* left, void* right) {
                return *static_cast<T*>(left) == *static_cast<T*>(right);
            }

           public:
            DEFAULT_METHODS(Compare)

            template <class T>
            Compare(T&& t)
                : APPLY2_FN(compare),
                  Box(std::forward<T>(t)) {}

            bool operator==(const Compare& right) const {
                if (!this->refptr || !right.refptr) {
                    return false;
                }
                if (compare_ptr != right.compare_ptr) {
                    return false;
                }
                return compare_ptr(this->ptr(), right.ptr());
            }
        };

        template <class Box>
        struct Compare2 : Box {
           private:
            bool (*compare1)(void* right, void* left) = nullptr;
            bool (*compare2)(void* right, void* left) = nullptr;

            template <class T, class U>
            static bool compare_fn(void* left, void* right) {
                return *static_cast<T*>(left) == *static_cast<U*>(right);
            }

           public:
            DEFAULT_METHODS(Compare2)

            template <class T, class U>
            Compare2(T&& t, U&& u)
                : compare1(compare_fn<std::remove_cvref_t<T>, std::remove_cvref_t<U>>),
                  compare2(compare_fn<std::remove_cvref_t<U>, std::remove_cvref_t<T>>),
                  Box(std::forward<T>(t)) {}

            bool operator==(const Compare2& right) const {
                if (!this->ptr() || !right.ptr()) {
                    return false;
                }
                // T U
                if (compare1 == right.compare2) {
                    return compare1(this->ptr(), right.ptr());
                }
                // U T
                if (compare2 == right.compare1) {
                    return compare2(right.ptr(), this->ptr());
                }
                return false;
            }
        };

        template <class Box, class T>
        using ForwardIterator = Compare<Increment<Deref<Box, T>>>;

        template <class Box, class T>
        using ForwardIterator2 = Compare2<Increment<Deref<Box, T>>>;

        template <class T1, class T2, class Box>
        struct Pair : Box {
           private:
            T1(*get_0_ptr)
            (void*) = nullptr;
            T2(*get_1_ptr)
            (void*) = nullptr;

            template <class Ret, size_t N, class T>
            static Ret get_N_fn(void* p) {
                auto ptr = static_cast<T*>(p);
                return get<N>(*ptr);
            }

           public:
            DEFAULT_METHODS(Pair)
            template <class T>
            Pair(T&& t)
                : get_0_ptr(get_N_fn<T1, 0, std::remove_cvref_t<T>>),
                  get_1_ptr(get_N_fn<T2, 1, std::remove_cvref_t<T>>),
                  Box(std::forward<T>(t)) {}

            T1 get_0() const {
                DEFAULT_CALL(get_0, select_traits<T1>());
            }

            T2 get_1() const {
                DEFAULT_CALL(get_1, select_traits<T2>());
            }
        };

        namespace internal {
            template <size_t>
            struct CallIface;
            template <>
            struct CallIface<0> {
                static decltype(auto) get(auto& p) {
                    return p.get_0();
                }
            };
            template <>
            struct CallIface<1> {
                static decltype(auto) get(auto& p) {
                    return p.get_1();
                }
            };
        }  // namespace internal

        template <size_t N, class T, class U, class Box>
        decltype(auto) get(Pair<T, U, Box>& b) {
            return internal::CallIface<N>::get(b);
        }

    }  // namespace iface
}  // namespace utils

#include "undef_macros.h"  // clear macros
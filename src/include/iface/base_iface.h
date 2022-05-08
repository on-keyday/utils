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
#define DEFAULT_METHODS(Type)                         \
    constexpr Type() = default;                       \
    constexpr Type(Type&) = default;                  \
    constexpr Type(const Type&) = default;            \
    constexpr Type(Type&&) = default;                 \
    constexpr Type& operator=(const Type&) = default; \
    constexpr Type& operator=(Type&&) = default;

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

            template <class T, class Fn>
            constexpr Ref(T& t, Fn&& from)
                : refptr(from(t)) {}

            constexpr void* ptr() const {
                return refptr;
            }
        };

        template <class T>
        constexpr bool is_narrow_enough = sizeof(T) <= sizeof(void*) &&
                                          alignof(T) <= alignof(void*);

        template <class T>
        void* select_pointer(T ptr) {
            using T_ = std::remove_cvref_t<T>;
            return new T_{std::move(ptr)};
        }

        using deleter_t = void (*)(void*);

        template <class T>
        deleter_t select_deleter() {
            return [](void* p) {
                delete static_cast<T*>(p);
            };
        }

        struct Owns : Ref {
           private:
            deleter_t deleter = nullptr;

           public:
            constexpr Owns() = default;

            template <class T>
            constexpr Owns(const T& t)
                : deleter(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(t, select_pointer<const T&>) {}

            template <class T, std::enable_if_t<!std::is_lvalue_reference_v<T>, int> = 0>
            constexpr Owns(T&& t)
                : deleter(select_deleter<std::remove_cvref_t<T>>()),
                  Ref(t, select_pointer<T&&>) {}

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
#define MAKE_FN_VOID(func_name, ...)                                     \
    void (*func_name##_ptr)(void* __VA_OPT__(, ) __VA_ARGS__) = nullptr; \
    template <class T, class... Args>                                    \
    static void func_name##_fn(void* ptr, Args... args) {                \
        static_cast<T*>(ptr)->func_name(std::forward<Args>(args)...);    \
    }
#define APPLY1_FN(func_name, ...) func_name##_fn<std::remove_cvref_t<T> __VA_OPT__(, ) __VA_ARGS__>
#define APPLY2_FN(func_name, ...) \
    func_name##_ptr(APPLY1_FN(func_name, __VA_ARGS__))
#define DEFAULT_CALL(func_name, defaultv, ...) return this->refptr && func_name##_ptr ? func_name##_ptr(this->refptr __VA_OPT__(, ) __VA_ARGS__) : defaultv;

        template <class Box>
        struct Sized : Box {
           private:
            MAKE_FN(size, size_t)
           public:
            DEFAULT_METHODS(Sized)

            template <class T>
            constexpr Sized(T&& t)
                : APPLY2_FN(size), Box(t) {}

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
                  Box(t) {}

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
                  Box(t) {}

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
                  Box(t) {}

            void erase(size_t offset, size_t count) {
                DEFAULT_CALL(erase, (void)0, offset, count)
            }

            void pop_back() {
                DEFAULT_CALL(pop_back, (void)0)
            }
        };

        template <class Box>
        struct Copy : Box {
           private:
            Copy (*copy_ptr)(void*) = nullptr;

            template <class T>
            static Copy copy_fn(void* p) {
                return *static_cast<T*>(p);
            }

           public:
            DEFAULT_METHODS(Copy)

            template <class T>
            Copy(T&& t)
                : APPLY2_FN(copy),
                  Box(t) {}

            Copy clone() const {
                return this->refptr && copy_ptr ? copy_ptr(this->refptr) : Copy{};
            }
        };

        template <class Value>
        Value select_traits() {
            if constexpr (std::is_reference_v<Value>) {
                int* fatal = 0;
                *fatal = 0;
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
                  Box(t) {}

            Value operator[](Key index) const {
                return subscript_ptr ? subscript_ptr(this->refptr, std::move(index)) : select_traits<Value>();
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
                  Box(t) {}

            Value operator*() const {
                return deref_ptr ? deref_ptr(this->refptr) : select_traits<Value>();
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
                  Box(t) {}

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
                  Box(t) {}

            bool operator==(const Compare& right) const {
                if (!this->refptr || !right.refptr) {
                    return false;
                }
                if (compare_ptr != right.compare_ptr) {
                    return false;
                }
                return compare_ptr(this->refptr, right.refptr);
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
                  Box(t) {}

            bool operator==(const Compare2& right) const {
                if (!this->refptr || !right.refptr) {
                    return false;
                }
                // T U
                if (compare1 == right.compare2) {
                    return compare1(this->refptr, right.refptr);
                }
                // U T
                if (compare2 == right.compare1) {
                    return compare2(right.refptr, this->refptr);
                }
                return false;
            }
        };

        template <class Box, class T>
        using ForwardIterator = Compare<Increment<Deref<Box, T>>>;

        template <class Box, class T>
        using ForwardIterator2 = Compare2<Increment<Deref<Box, T>>>;
    }  // namespace iface
}  // namespace utils

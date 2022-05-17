/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// base_iface - basic interface class
#pragma once
#include <memory>
#include <type_traits>
#include <cassert>

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
                return types != 0;
            }

            ~Powns() {
                if (auto del = get_deleter(types)) {
                    del(this->ptr(), nullptr);
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
#define DEFAULT_CALL(func_name, defaultv, ...) return this->ptr() && func_name##_ptr ? func_name##_ptr(this->ptr() __VA_OPT__(, ) __VA_ARGS__) : defaultv;

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
                  Box(std::forward<T>(t)) {}

            Copy clone() const {
                DEFAULT_CALL(copy, Copy{})
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
            static Ret get_N_fn(void* ptr) {
                auto ptr = static_cast<T*>(ptr);
                return get<N>(*ptr);
            }

           public:
            DEFAULT_METHODS(Pair)
            template <class T>
            Pair(T&& t)
                : get_0_ptr(get_N_fn<T1, 0, std::remove_cvref_t<T>>),
                  get_1_ptr(get_N_fn<T2, 2, std::remove_cvref_t<T>>),
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
            class CallIface;
            template <>
            class CallIface<0> {
                decltype(auto) get(auto& p) {
                    return p.get_0();
                }
            };
            template <>
            class CallIface<1> {
                decltype(auto) get(auto& p) {
                    return p.get_1();
                }
            };
        }  // namespace internal

        template <size_t N, class T, class U, class Box>
        decltype(auto) get(Pair<T, U, Box>& b) {
            return internal::CallIface<N>::get(b);
        }

        namespace internal {
            template <class T>
            concept has_code = requires(T t) {
                { t.code() } -> std::integral;
            };

            template <class T>
            concept has_kind_of = requires(T t) {
                {!t.kind_of("kind")};
            };

            template <class T>
            concept has_unwrap = requires(T t) {
                {t.unwrap()};
            };
        }  // namespace internal

        template <size_t N>
        struct fixedBuf {
            char buf[N + 1]{0};
            size_t index = 0;
            const char* c_str() const {
                return buf;
            }
            void push_back(char v) {
                if (index >= N) {
                    return;
                }
                buf[index] = v;
                index++;
            }
            char operator[](size_t i) {
                if (index >= i) {
                    return char();
                }
                return buf[i];
            }
        };

        template <class Box, class Unwrap = void>
        struct Error : Box {
           private:
            using unwrap_t = std::conditional_t<std::is_same_v<Unwrap, void>, Error, Unwrap>;
            void (*error_ptr)(void*, PushBacker<Ref> msg) = nullptr;
            std::int64_t (*code_ptr)(void*) = nullptr;
            bool (*kind_of_ptr)(void*, const char* key) = nullptr;
            unwrap_t (*unwrap_ptr)(void*) = nullptr;

            template <class T>
            static void error_fn(void* ptr, PushBacker<Ref> msg) {
                return static_cast<T*>(ptr)->error(msg);
            }

            template <class T>
            static std::int64_t code_fn(void* ptr) {
                if constexpr (internal::has_code<T>) {
                    return static_cast<T*>(ptr)->code();
                }
                else {
                    return false;
                }
            }

            template <class T>
            static bool kind_of_fn(void* ptr, const char* kind) {
                if constexpr (internal::has_kind_of<T>) {
                    return static_cast<T*>(ptr)->kind_of(kind);
                }
                else {
                    return false;
                }
            }

            template <class T>
            static unwrap_t unwrap_fn(void* ptr) {
                if constexpr (internal::has_unwrap<T>) {
                    return static_cast<T*>(ptr)->unwrap();
                }
                else {
                    return {};
                }
            }

           public:
            DEFAULT_METHODS(Error)

            template <class T>
            Error(T&& t)
                : APPLY2_FN(error),
                  APPLY2_FN(code),
                  APPLY2_FN(kind_of),
                  APPLY2_FN(unwrap),
                  Box(std::forward<T>(t)) {}

            void error(PushBacker<Ref> ref) const {
                DEFAULT_CALL(error, (void)0, ref);
            }

            template <class String = fixedBuf<100>>
            String serror() const {
                String s;
                error(s);
                return s;
            }

            std::int64_t code() const {
                DEFAULT_CALL(code, 0);
            }

            bool kind_of(const char* kind) const {
                DEFAULT_CALL(kind_of, false, kind);
            }

            unwrap_t unwrap() const {
                DEFAULT_CALL(unwrap, unwrap_t{});
            }
        };
    }  // namespace iface
}  // namespace utils

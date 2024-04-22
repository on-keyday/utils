/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// error - Error for generic error handling
#pragma once
#include <memory>
#include <atomic>
#include <concepts>
#include <bit>

#include <core/byte.h>
#include <view/iovec.h>
#include <helper/pushbacker.h>
#include <helper/defer.h>
#include <number/to_string.h>
#include <strutil/append.h>
#include <helper/disable_self.h>
#include <helper/template_instance.h>

namespace futils::error {

    enum class ErrorType : byte {
        null,
        c_str,
        number,
        ptr,
    };

    enum class Category : byte {
        none,
        bad_alloc,
        os,
        lib,
        app,
    };

    struct ErrorTraits {
        bool has_category = false;
        bool has_subcategory = false;
        bool has_equal = false;
        bool has_code = false;
        bool has_unwrap = false;
    };

    namespace internal {
        enum class ReflectorOp {
            error,
            decref,
            incref,
            category,
            sub_category,
            unwrap,
            pointer,
            code,
            traits,
            equal,
            compare_fn,
        };

        struct WrapperBase;

        using reflector_t = std::uint64_t (*)(WrapperBase* self, ReflectorOp, std::uintptr_t, std::uintptr_t);

        using compare_t = bool (*)(const void* a, const void* b);

        template <class T>
        concept has_error = requires(T t) {
            { t.error(std::declval<helper::IPushBacker<>>()) };
        };

        template <class T>
        concept has_category = requires(T t) {
            { t.category() } -> std::same_as<Category>;
        };

        template <class T>
        concept has_sub_category = requires(T t) {
            { t.sub_category() } -> std::convertible_to<std::uint32_t>;
        };

        template <class T>
        concept has_code_fn = requires(T t) {
            { t.code() } -> std::convertible_to<std::int64_t>;
        };

        template <class T>
        concept has_code_var = requires(T t) {
            { t.code } -> std::convertible_to<std::int64_t>;
        };

        template <class T>
        concept has_equal = requires(const T& t) {
            { t == t } -> std::convertible_to<bool>;
        };

        template <class T, class E>
        concept has_unwrap = requires(T t) {
            { t.unwrap() } -> std::convertible_to<E>;
        };

        template <class T>
        constexpr bool compare(const void* a, const void* b) {
            if constexpr (has_equal<T>) {
                return *static_cast<const T*>(a) == *static_cast<const T*>(b);
            }
            else {
                return std::is_empty_v<T>;
            }
        }

        struct WrapperBase {
           protected:
            reflector_t reflector = nullptr;

            constexpr WrapperBase(reflector_t r)
                : reflector(r){};

           public:
            void error(helper::IPushBacker<> pb) {
                reflector(this, ReflectorOp::error, std::bit_cast<std::uintptr_t>(&pb), 0);
            }

            void incref() {
                reflector(this, ReflectorOp::incref, 0, 0);
            }

            void decref() {
                reflector(this, ReflectorOp::decref, 0, 0);
            }

            Category category() {
                return static_cast<Category>(reflector(this, ReflectorOp::category, 0, 0));
            }

            std::uint32_t sub_category() {
                return static_cast<std::uint32_t>(reflector(this, ReflectorOp::sub_category, 0, 0));
            }

            std::int64_t code() {
                return static_cast<std::int64_t>(reflector(this, ReflectorOp::code, 0, 0));
            }

            ErrorTraits traits() {
                ErrorTraits traits;
                reflector(this, ReflectorOp::traits, std::bit_cast<std::uintptr_t>(&traits), 0);
                return traits;
            }

            template <class E>
            E unwrap() {
                E e;
                // HACK(on-keyday): because all variant of Error template instance has same memory layout
                //                  and same semantics of field, we can use this hack.
                reflector(this, ReflectorOp::unwrap, std::bit_cast<std::uintptr_t>(&e), 0);
                return e;
            }

            template <class T>
            T* pointer() {
                return std::bit_cast<T*>(std::uintptr_t(reflector(this, ReflectorOp::pointer, 0, 0)));
            }

            template <class T>
            const T* pointer() const {
                return std::bit_cast<const T*>(std::uintptr_t(reflector(const_cast<WrapperBase*>(this), ReflectorOp::pointer, 0, 0)));
            }

            reflector_t get_reflector() {
                return reflector;
            }

            compare_t get_compare() const {
                compare_t cmp = nullptr;
                reflector(nullptr, ReflectorOp::compare_fn, std::bit_cast<std::uintptr_t>(&cmp), 0);
                return cmp;
            }

            friend bool operator==(const WrapperBase& a, const WrapperBase& b) {
                if (&a == &b) {
                    return true;
                }
                if (a.reflector != b.reflector) {
                    auto cmp1 = a.get_compare();
                    auto cmp2 = b.get_compare();
                    if (cmp1 != cmp2) {
                        return false;
                    }
                    return cmp1(a.pointer<void>(), b.pointer<void>());
                }
                return a.reflector(nullptr, ReflectorOp::equal, std::bit_cast<std::uintptr_t>(&a), std::bit_cast<std::uintptr_t>(&b)) != 0;
            }
        };

        template <class T, class E, class RefCount>
        struct WrapperT
            : WrapperBase,
              helper::omit_empty<typename std::allocator_traits<
                  typename E::allocator_type>::template rebind_alloc<WrapperT<T, E, RefCount>>> {
            using Alloc = typename std::allocator_traits<
                typename E::allocator_type>::template rebind_alloc<WrapperT<T, E, RefCount>>;
            using traits = typename std::allocator_traits<typename E::allocator_type>::template rebind_traits<WrapperT>;
            T value;
            RefCount ref_count;

            static_assert(has_error<T>, "T must have error method");

            static std::uint64_t reflectorT(WrapperBase* p_self, ReflectorOp op, std::uintptr_t arg1, std::uintptr_t arg2) {
                WrapperT* self = static_cast<WrapperT*>(p_self);
                switch (op) {
                    default: {
                        return 0;
                    }
                    case ReflectorOp::equal: {
                        auto p1 = static_cast<const WrapperT*>(std::bit_cast<const void*>(arg1));
                        auto p2 = static_cast<const WrapperT*>(std::bit_cast<const void*>(arg2));
                        if constexpr (has_equal<T>) {
                            return p1->value == p2->value;
                        }
                        else {
                            return std::is_empty_v<T>;
                        }
                    }
                    case ReflectorOp::compare_fn: {
                        auto cmp = static_cast<compare_t*>(std::bit_cast<void*>(arg1));
                        *cmp = compare<T>;
                        return 0;
                    }
                    case ReflectorOp::error: {
                        helper::IPushBacker<>* pb = reinterpret_cast<helper::IPushBacker<>*>(arg1);
                        self->value.error(*pb);
                        return 0;
                    }
                    case ReflectorOp::incref: {
                        ++self->ref_count;
                        return 0;
                    }
                    case ReflectorOp::decref: {
                        if (--self->ref_count == 0) {
                            auto alloc = std::move(self->om_value());
                            traits::destroy(alloc, self);
                            traits::deallocate(alloc, self, 1);
                            return 1;
                        }
                        return 0;
                    }
                    case ReflectorOp::category: {
                        if constexpr (has_category<T>) {
                            return static_cast<std::uintptr_t>(self->value.category());
                        }
                        else {
                            return static_cast<std::uintptr_t>(Category::none);
                        }
                    }
                    case ReflectorOp::sub_category: {
                        if constexpr (has_sub_category<T>) {
                            return static_cast<std::uintptr_t>(self->value.sub_category());
                        }
                        else {
                            return 0;
                        }
                    }
                    case ReflectorOp::unwrap: {
                        if constexpr (has_unwrap<T, E>) {
                            E* e = reinterpret_cast<E*>(arg1);
                            *e = self->value.unwrap();
                            return 1;
                        }
                        else {
                            return 0;
                        }
                    }
                    case ReflectorOp::pointer: {
                        return std::bit_cast<std::uintptr_t>(std::addressof(self->value));
                    }
                    case ReflectorOp::code: {
                        if constexpr (has_code_fn<T>) {
                            return static_cast<std::int64_t>(self->value.code());
                        }
                        else if constexpr (has_code_var<T>) {
                            return static_cast<std::int64_t>(self->value.code);
                        }
                        else {
                            return 0;
                        }
                    }
                    case ReflectorOp::traits: {
                        ErrorTraits* traits = reinterpret_cast<ErrorTraits*>(arg1);
                        traits->has_category = has_category<T>;
                        traits->has_subcategory = has_sub_category<T>;
                        traits->has_equal = has_equal<T>;
                        traits->has_code = has_code_fn<T>;
                        traits->has_unwrap = has_unwrap<T, E>;
                        return 0;
                    }
                }
            }

            template <class A, class... Args>
            WrapperT(A&& alloc, Args&&... args)
                : WrapperBase(reflectorT),
                  helper::omit_empty<Alloc>(std::forward<A>(alloc)),
                  value(std::forward<Args>(args)...),
                  ref_count(1) {}
        };

        template <class T>
        concept has_to_string = requires(T t) {
            { to_string(t) } -> std::convertible_to<const char*>;
        };

    }  // namespace internal

    template <class SubCategory>
    struct DefaultFormatTraits {
        static constexpr void null_error(auto&& out, Category c, SubCategory s) {
            if (c == Category::bad_alloc) {
                strutil::append(out, "bad_alloc");
                return;
            }
            strutil::append(out, "<null>");
        }

        static constexpr void num_error(auto&& out, std::int64_t val, Category c, SubCategory s) {
            strutil::append(out, "code=");
            number::to_string(out, val);
        }

        static constexpr void str_error(auto&& out, const char* str, Category c, SubCategory s) {
            strutil::append(out, str);
        }

        static constexpr void categories(auto&& out, ErrorType t, Category c, SubCategory s) {
            if (t == ErrorType::ptr) {
                return;
            }
            strutil::append(out, " category=");
            switch (c) {
                case Category::none: {
                    strutil::append(out, "none");
                    break;
                }
                case Category::bad_alloc: {
                    strutil::append(out, "bad_alloc");
                    break;
                }
                case Category::os: {
                    strutil::append(out, "os");
                    break;
                }
                case Category::lib: {
                    strutil::append(out, "lib");
                    break;
                }
                case Category::app: {
                    strutil::append(out, "app");
                    break;
                }
                default: {
                    strutil::append(out, "unknown(");
                    number::to_string(out, static_cast<std::uint32_t>(c));
                    strutil::append(out, ")");
                    break;
                }
            }
            if constexpr (internal::has_to_string<SubCategory>) {
                if (auto str = to_string(s)) {
                    strutil::append(out, " sub_category=");
                    strutil::append(out, str);
                    return;
                }
            }
            if (s != SubCategory()) {
                strutil::append(out, " sub_category=");
                number::to_string(out, static_cast<std::uint32_t>(s));
            }
        }
    };

    /*** Error is a Golang-like error interface.
     Objects should implement void error(helper::IPushBacker<>),
     specialized to number and const char*.
     Support Error copy by reference count of internal pointer.
     Also supports Error unwrap() like Golang errors.Unwrap().
     Reflection like T* as() function is also supported.

     User-defined Error MUST implement:
       void error(helper::IPushBacker<>)

     User-defined Error MAY implement:
       error::Error unwrap()
       error::Category category()
       std::uint32_t sub_category()
       std::uint64_t code()

     @param Alloc Allocator for internal pointer.
     @param ErrorBuffer Error buffer type or void.
               if set not void, simple error() returns ErrorBuffer object.
               If used with helper::either::expected,
               and when an exception occurs, ErrorBuffer is used to store the error message.
               what() returns ErrorBuffer.c_str() if ErrorBuffer has a c_str() method.
     @param SubCategory Sub-category type for error message.
     @param FormatTraits Format traits for error message.
         FormatTraits must implement:
          static constexpr void null_error(auto&& out, Category c, SubCategory s)
          static constexpr void num_error(auto&& out, std::uint64_t val, Category c, SubCategory s)
          static constexpr void str_error(auto&& out, const char* str, Category c, SubCategory s)
          static constexpr void categories(auto&& out, ErrorType t, Category c, SubCategory s)
     @param RefCount Reference count type for internal pointer.
    */
    template <class Alloc = std::allocator<byte>, class ErrorBuffer = void, class SubCategory = std::uint32_t, class FormatTraits = DefaultFormatTraits<SubCategory>, class RefCount = std::atomic_uint32_t>
    struct Error {
        using error_buffer_type = ErrorBuffer;
        using allocator_type = Alloc;
        using traits = std::allocator_traits<Alloc>;

       private:
        template <class A, class B, class C, class F, class R>
        friend struct Error;

        ErrorType type_ = ErrorType::null;
        Category category_ = Category::none;
        SubCategory sub_category_ = 0;  // implementation defined
        union {
            std::int64_t number = 0;
            const char* c_str;
            internal::WrapperBase* ptr;
        };

        template <class A, class B, class C, class F, class R>
        constexpr void copy_data(const Error<A, B, C, F, R>& other) {
            type_ = other.type_;
            category_ = other.category_;
            sub_category_ = other.sub_category_;
            switch (type_) {
                case ErrorType::null: {
                    break;
                }
                case ErrorType::c_str: {
                    c_str = other.c_str;
                    break;
                }
                case ErrorType::number: {
                    number = other.number;
                    break;
                }
                case ErrorType::ptr: {
                    ptr = other.ptr;
                    ptr->incref();
                    break;
                }
            }
        }

        template <class A, class B, class C, class F, class R>
        constexpr void move_data(Error<A, B, C, F, R>&& other) {
            type_ = other.type_;
            category_ = other.category_;
            sub_category_ = other.sub_category_;
            switch (type_) {
                case ErrorType::null: {
                    break;
                }
                case ErrorType::c_str: {
                    c_str = other.c_str;
                    other.c_str = nullptr;
                    break;
                }
                case ErrorType::number: {
                    number = other.number;
                    other.number = 0;
                    break;
                }
                case ErrorType::ptr: {
                    ptr = other.ptr;
                    other.ptr = nullptr;
                    break;
                }
            }
            other.type_ = ErrorType::null;
            other.category_ = Category::none;
            other.sub_category_ = 0;
        }

       public:
        constexpr Error() = default;

        constexpr void reset() {
            switch (type_) {
                case ErrorType::null: {
                    break;
                }
                case ErrorType::c_str: {
                    c_str = nullptr;
                    break;
                }
                case ErrorType::number: {
                    number = 0;
                    break;
                }
                case ErrorType::ptr: {
                    ptr->decref();
                    ptr = nullptr;
                    break;
                }
            }
            type_ = ErrorType::null;
            category_ = Category::none;
            sub_category_ = 0;
        }

        constexpr Error(const Error& other) {
            copy_data(other);
        }

        constexpr Error(Error&& other) {
            move_data(std::move(other));
        }

        constexpr Error& operator=(const Error& other) {
            if (this != &other) {
                reset();
                copy_data(other);
            }
            return *this;
        }

        constexpr Error& operator=(Error&& other) {
            if (this != &other) {
                reset();
                move_data(std::move(other));
            }
            return *this;
        }

        constexpr ~Error() {
            reset();
        }

        constexpr Error(Category category, SubCategory sub_category = SubCategory())
            : type_(ErrorType::null),
              category_(category),
              sub_category_(sub_category) {}

        constexpr Error(const char* str, Category category, SubCategory sub_category = SubCategory())
            : type_(ErrorType::c_str),
              category_(category),
              sub_category_(sub_category),
              c_str(str) {}

        constexpr Error(std::uint64_t number, Category category, SubCategory sub_category = SubCategory())
            : type_(ErrorType::number),
              category_(category),
              sub_category_(sub_category),
              number(number) {}

       private:
        template <class T>
        constexpr void construct_ptr(T&& t, Alloc&& a) {
            type_ = ErrorType::ptr;
            using wrapper_t = internal::WrapperT<std::decay_t<T>, Error, RefCount>;
            using wrapper_traits_t = typename wrapper_t::traits;
            using wrapper_alloc_t = typename wrapper_traits_t::allocator_type;
            wrapper_alloc_t alloc{std::forward<Alloc>(a)};
            auto fallback = helper::defer([&] {
                type_ = ErrorType::null;
                category_ = Category::bad_alloc;
            });
            wrapper_t* wrapper = wrapper_traits_t::allocate(alloc, 1);
            if (!wrapper) {
                return;
            }
            fallback.cancel();
            auto safe = helper::defer([&] {
                wrapper_traits_t::deallocate(alloc, wrapper, 1);
            });
            wrapper_traits_t::construct(alloc, wrapper, alloc, std::forward<T>(t));
            safe.cancel();
            ptr = wrapper;
            auto tr = ptr->traits();

            if (tr.has_category) {
                category_ = ptr->category();
            }
            else {
                category_ = Category::none;
            }
            if (tr.has_subcategory) {
                sub_category_ = SubCategory(ptr->sub_category());
            }
        }

       public:
        template <class T>
            requires(internal::has_error<T> && !helper::is_template_instance_of<std::decay_t<T>, Error>)
        constexpr Error(T&& t) {
            construct_ptr(std::forward<T>(t), Alloc{});
        }

        template <class T, class A>
            requires(internal::has_error<T> && !helper::is_template_instance_of<std::decay_t<T>, Error>)
        constexpr Error(T&& t, A&& a) {
            construct_ptr(std::forward<T>(t), std::forward<A>(a));
        }

        template <class T, class A>
            requires(internal::has_error<T> && !helper::is_template_instance_of<std::decay_t<T>, Error>)
        constexpr Error(T&& t, Category category, SubCategory sub_category = SubCategory(), A&& a = Alloc{})
            : Error(std::forward<T>(t), std::forward<A>(a)) {
            this->category_ = category;
            this->sub_category_ = sub_category;
        }

        template <class A, class B, class C, class F, class R>
            requires(!std::is_same_v<Error, Error<A, B, C, F, R>>)
        constexpr Error(const Error<A, B, C, F, R>& other) {
            copy_data(other);
        }

        template <class A, class B, class C, class F, class R>
            requires(!std::is_same_v<Error, Error<A, B, C, F, R>>)
        constexpr Error(Error<A, B, C, F, R>&& other) {
            move_data(std::move(other));
        }

        constexpr ErrorType type() const {
            return type_;
        }

        constexpr Category category() const {
            return category_;
        }

        constexpr SubCategory sub_category() const {
            return sub_category_;
        }

        constexpr std::int64_t code() const {
            switch (type_) {
                case ErrorType::null: {
                    return 0;
                }
                case ErrorType::c_str: {
                    return 0;
                }
                case ErrorType::number: {
                    return number;
                }
                case ErrorType::ptr: {
                    return ptr->code();
                }
            }
        }

        constexpr ErrorTraits error_traits() const {
            switch (type_) {
                case ErrorType::null: {
                    return ErrorTraits{};
                }
                case ErrorType::c_str: {
                    return ErrorTraits{.has_category = true, .has_subcategory = true, .has_equal = true, .has_code = false, .has_unwrap = false};
                }
                case ErrorType::number: {
                    return ErrorTraits{.has_category = true, .has_subcategory = true, .has_equal = true, .has_code = true, .has_unwrap = false};
                }
                case ErrorType::ptr: {
                    return ptr->traits();
                }
            }
            return ErrorTraits{};
        }

        template <class A, class B, class C, class F, class R>
        constexpr bool operator==(const Error<A, B, C, F, R>& other) const {
            if constexpr (std::is_same_v<Error, Error<A, B, C, F, R>>) {
                if (this == &other) {
                    return true;
                }
            }
            if (type_ != other.type_) {
                return false;
            }
            if (category_ != other.category_) {
                return false;
            }
            if (sub_category_ != other.sub_category_) {
                return false;
            }
            switch (type_) {
                case ErrorType::null: {
                    return true;
                }
                case ErrorType::c_str: {
                    return view::basic_rvec<char>(c_str) == view::basic_rvec<char>(other.c_str);
                }
                case ErrorType::number: {
                    return number == other.number;
                }
                case ErrorType::ptr: {
                    return *ptr == *other.ptr;
                }
            }
            return false;
        }

        constexpr Error unwrap() const {
            if (type_ == ErrorType::ptr) {
                return ptr->unwrap<Error>();
            }
            return Error{};
        }

        // as() is a reflection function like Golang's type assertion.
        // It returns pointer to T if T is the same type as the internal pointer.
        // Otherwise, it returns nullptr.
        template <class T>
        constexpr T* as() {
            if (type_ != ErrorType::ptr) {
                return nullptr;
            }
            internal::reflector_t cmp_reflector = internal::WrapperT<T, Error, RefCount>::reflectorT;
            if (ptr->get_reflector() != cmp_reflector) {
                internal::compare_t cmp = nullptr;
                cmp_reflector(nullptr, internal::ReflectorOp::compare_fn, std::bit_cast<std::uintptr_t>(&cmp), 0);
                auto self = ptr->get_compare();
                if (self != cmp) {
                    return nullptr;
                }
            }
            return ptr->template pointer<T>();
        }

        template <class T>
        constexpr const T* as() const {
            return const_cast<const T*>(const_cast<Error*>(this)->template as<T>());
        }

        constexpr void* unsafe_ptr() {
            if (type_ != ErrorType::ptr) {
                return nullptr;
            }
            return ptr->pointer<void>();
        }

        constexpr const void* unsafe_ptr() const {
            return const_cast<const void*>(const_cast<Error*>(this)->unsafe_ptr());
        }

        constexpr void error(auto&& out) const {
            switch (type_) {
                case ErrorType::null: {
                    FormatTraits::null_error(out, category_, sub_category_);
                    break;
                }
                case ErrorType::c_str: {
                    FormatTraits::str_error(out, c_str, category_, sub_category_);
                    break;
                }
                case ErrorType::number: {
                    FormatTraits::num_error(out, number, category_, sub_category_);
                    break;
                }
                case ErrorType::ptr: {
                    ptr->error(out);
                    break;
                }
            }
            FormatTraits::categories(out, type_, category_, sub_category_);
        }

        template <class T>
        constexpr T error() const {
            T t;
            error(t);
            return t;
        }

        constexpr ErrorBuffer error()
            requires(std::is_default_constructible_v<ErrorBuffer>)
        {
            return error<ErrorBuffer>();
        }

        constexpr operator bool() const noexcept {
            return has_error();
        }

        constexpr bool has_error() const noexcept {
            return type_ != ErrorType::null || category_ != Category::none;
        }

        constexpr bool has_no_error() const noexcept {
            return !has_error();
        }
    };

    template <class Err = Error<>>
    struct ErrList {
        Err err;
        Err before;

        constexpr void error(auto&& pb) {
            if (before) {
                before.error(pb);
                pb.push_back(',');
            }
            err.error(pb);
        }

        constexpr Err unwrap() {
            return before;
        }
    };
}  // namespace futils::error

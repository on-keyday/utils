/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// reflect - lite runtime reflection
// not have interface for name access
// but write binary directly
#pragma once
#include <type_traits>
#include <tuple>
#include "../binary/number.h"

namespace futils {
    namespace reflect {
        enum class Traits {
            size,
            align,
            is_integral,
            is_floating,
            is_enum,
            is_union,
            is_struct,
            is_abstract,
            is_pointer,
            is_std_layout,
            is_empty,
            is_array,
            is_signed,
            is_unsigned,
            is_polymorphic,

            // internal
            underlying_ptr,
            underlying_const,
            underlying_fn,
        };

        using Type = std::uintptr_t (*)(Traits, const void*, size_t);
        namespace internal {
            template <class A, size_t idx>
            std::uintptr_t access(A (*ptr)[idx], size_t set) {
                if (set >= idx) {
                    return 0;
                }
                return std::uintptr_t(std::addressof((*ptr)[set]));
            }
        }  // namespace internal

        template <class T>
        std::uintptr_t type(Traits traits, const void* ptr, size_t idx) {
            if (traits == Traits::size) {
                return sizeof(T);
            }
            else if (traits == Traits::align) {
                return alignof(T);
            }
            else if (traits == Traits::is_integral) {
                return std::is_integral_v<T>;
            }
            else if (traits == Traits::is_floating) {
                return std::is_floating_point_v<T>;
            }
            else if (traits == Traits::is_enum) {
                return std::is_enum_v<T>;
            }
            else if (traits == Traits::is_abstract) {
                return std::is_abstract_v<T>;
            }
            else if (traits == Traits::is_pointer) {
                return std::is_pointer_v<T>;
            }
            else if (traits == Traits::is_struct) {
                return std::is_class_v<T>;
            }
            else if (traits == Traits::is_std_layout) {
                return std::is_standard_layout_v<T>;
            }
            else if (traits == Traits::is_union) {
                return std::is_union_v<T>;
            }
            else if (traits == Traits::is_empty) {
                return std::is_empty_v<T>;
            }
            else if (traits == Traits::is_array) {
                return std::is_array_v<T>;
            }
            else if (traits == Traits::is_signed) {
                return std::is_signed_v<T>;
            }
            else if (traits == Traits::is_unsigned) {
                return std::is_unsigned_v<T>;
            }
            else if (traits == Traits::is_polymorphic) {
                return std::is_polymorphic_v<T>;
            }
            else if (traits == Traits::underlying_ptr) {
                if constexpr (std::is_pointer_v<T>) {
                    return std::uintptr_t(*static_cast<const T*>(ptr));
                }
                else if constexpr (std::is_enum_v<T>) {
                    return std::uintptr_t(ptr);
                }
                else if constexpr (std::is_array_v<T>) {
                    return internal::access(static_cast<T*>(const_cast<void*>(ptr)), idx);
                }
            }
            else if (traits == Traits::underlying_const) {
                if constexpr (std::is_pointer_v<T>) {
                    return std::is_const_v<std::remove_pointer_t<T>>;
                }
            }
            else if (traits == Traits::underlying_fn) {
                auto v = static_cast<Type*>(const_cast<void*>(ptr));
                if constexpr (std::is_pointer_v<T>) {
                    *v = type<std::remove_const_t<std::remove_pointer_t<T>>>;
                }
                else if constexpr (std::is_enum_v<T>) {
                    *v = type<std::underlying_type_t<T>>;
                }
                else if constexpr (std::is_array_v<T>) {
                    *v = type<std::remove_const_t<std::remove_extent_t<T>>>;
                }
                else {
                    return 0;
                }
                return 1;
            }
            return 0;
        }

        struct Reflect;

        template <class T, class From, class... Other>
        bool assign(void* target, Reflect&);

        namespace test {
            struct Test1 {
                byte d1;
                byte d2;
                std::uint16_t d3;
            };
            constexpr auto test1_size = sizeof(Test1);
            constexpr auto test1_align = alignof(Test1);
            struct Test2 {
                byte d1;
                std::uint16_t d2;
                byte d3;
            };
            constexpr auto test2_size = sizeof(Test2);
            constexpr auto test2_align = alignof(Test2);

            struct Test3 {
                std::uintptr_t val = 0;
                virtual void nop() {}
                virtual void op() {}
                virtual ~Test3() {}
            };
            constexpr auto test3_size = sizeof(Test3);
            constexpr auto test3_align = alignof(Test3);

            struct Test4 : Test3 {
                std::uintptr_t val2 = 0;
                virtual ~Test4() {}
            };

            constexpr auto test4_size = sizeof(Test4);
            constexpr auto test4_align = alignof(Test4);

        }  // namespace test

        using vtable_fn_t = void (*)(const void* instance);
        struct Reflect {
           private:
            Type type = nullptr;
            const void* ptr = nullptr;
            bool is_rval_ = false;
            bool is_const_ = false;

           public:
            constexpr Reflect() = default;

            template <class T>
            constexpr Reflect(T&& t) {
                scan(std::forward<T>(t));
            }

            const byte* unsafe_ptr(size_t offset = 0) const {
                if (offset >= size()) {
                    return nullptr;
                }
                return static_cast<const byte*>(ptr) + offset;
            }

            const byte* unsafe_ptr_aligned(size_t nalign) const {
                return unsafe_ptr(align() * nalign);
            }

            template <class T>
            constexpr bool is() const {
                return reflect::type<T> == type;
            }

            constexpr std::uintptr_t traits(Traits t) const {
                return type ? type(t, nullptr, 0) : 0;
            }

            constexpr size_t size() const {
                return traits(Traits::size);
            }

            constexpr size_t align() const {
                return traits(Traits::align);
            }

            constexpr bool is_const() const {
                return is_const_;
            }

            constexpr bool is_rval() const {
                return is_rval_;
            }

            constexpr bool underlying(Reflect& ref, size_t idx = 0) const {
                if (!traits(Traits::is_pointer) &&
                    !traits(Traits::is_enum) &&
                    !traits(Traits::is_array)) {
                    return false;
                }
                Type under_fn = nullptr;
                if (!type(Traits::underlying_fn, &under_fn, idx) || !under_fn) {
                    return false;
                }
                auto under_ptr = reinterpret_cast<const void*>(type(Traits::underlying_ptr, ptr, idx));
                if (!under_ptr) {
                    return false;
                }
                auto under_const = type(Traits::underlying_const, nullptr, idx);
                ref.ptr = under_ptr;
                ref.type = under_fn;
                ref.is_const_ = under_const;
                ref.is_rval_ = is_rval_;
                return true;
            }

           private:
            template <size_t v, class A>
            constexpr void scan_array(A (&&a)[v]) {
                type = reflect::type<A[v]>;
                is_const_ = std::is_const_v<A>;
                is_rval_ = true;
                ptr = a;
            }

            template <size_t v, class A>
            constexpr void scan_array(A (&a)[v]) {
                type = reflect::type<A[v]>;
                is_const_ = std::is_const_v<A>;
                is_rval_ = false;
                ptr = a;
            }

           public:
            constexpr void scan(auto&& arg) {
                if constexpr (std::is_bounded_array_v<std::remove_reference_t<decltype(arg)>>) {
                    return scan_array(arg);
                }
                else {
                    using T = std::decay_t<decltype(arg)>;
                    type = reflect::type<T>;
                    is_const_ = std::is_const_v<std::remove_reference_t<decltype(arg)>>;
                    is_rval_ = std::is_rvalue_reference_v<decltype(arg)>;
                    ptr = std::addressof(arg);
                }
            }

            template <class T>
            bool visit(auto&& cb) const {
                if (!is<T>()) {
                    return false;
                }
                if (is_const_) {
                    cb(*static_cast<const T*>(ptr));
                    return true;
                }
                else if (is_rval_) {
                    cb(std::move(*static_cast<T*>(const_cast<void*>(ptr))));
                }
                else {
                    cb(*static_cast<T*>(const_cast<void*>(ptr)));
                }
                return true;
            }

           private:
            template <class T>
            constexpr bool valid_offset(size_t offset) const {
                if (!traits(Traits::is_struct) &&
                    !traits(Traits::is_union) &&
                    (offset != 0 || sizeof(T) != size())) {
                    return false;
                }
                return true;
            }

           public:
            template <class T>
            bool get(T& val, size_t offset = 0) const {
                if (offset >= size()) {
                    return false;
                }
                if (!valid_offset<T>(offset)) {
                    return false;
                }
                binary::reader r{view::rvec(static_cast<const byte*>(ptr), size())};
                auto alignptr = r.remain().data() + offset;
                if (std::uintptr_t(alignptr) % alignof(T) != 0) {
                    return false;
                }
                r.offset(offset);
                if constexpr (std::is_pointer_v<T>) {
                    std::uintptr_t tmp;
                    if (!binary::read_num(r, tmp, !binary::is_little())) {
                        return false;
                    }
                    val = reinterpret_cast<T>(tmp);
                    return true;
                }
                else {
                    return binary::read_num(r, val, !binary::is_little());
                }
            }

            template <class T>
            bool get_aligned(T& val, size_t nalign) {
                return get(val, align() * nalign);
            }

            template <class T>
            bool set(T val, size_t offset = 0) const {
                if (is_const_ || offset >= size()) {
                    return false;
                }
                if (!valid_offset<T>(offset)) {
                    return false;
                }
                binary::writer w{view::wvec(static_cast<byte*>(const_cast<void*>(ptr)), size())};
                auto alignptr = w.remain().data() + offset;
                if (std::uintptr_t(alignptr) % alignof(T) != 0) {
                    return false;
                }
                w.offset(offset);
                if constexpr (std::is_pointer_v<T>) {
                    return binary::write_num(w, std::uintptr_t(val), !binary::is_little());
                }
                else {
                    return binary::write_num(w, val, !binary::is_little());
                }
            }

            template <class T>
            bool set_aligned(T val, size_t nalign) {
                return set(val, align() * nalign);
            }

            constexpr size_t num_aligns() const {
                if (align() == 0) {
                    return 0;
                }
                return size() / align();
            }

            constexpr size_t num_elements() const {
                if (!traits(Traits::is_array)) {
                    return 0;
                }
                Reflect elm;
                if (!underlying(elm)) {
                    return 0;
                }
                if (elm.size() == 0) {
                    return 0;
                }
                return size() / elm.size();
            }

            constexpr bool is_int() const {
                return traits(Traits::is_signed);
            }

            constexpr bool is_uint() const {
                return traits(Traits::is_unsigned);
            }

            std::int64_t get_int() const {
                if (!is_int()) {
                    return 0;
                }
                switch (size()) {
                    case 1: {
                        std::int8_t d;
                        get(d, 0);
                        return d;
                    }
                    case 2: {
                        std::int16_t d;
                        get(d, 0);
                        return d;
                    }
                    case 4: {
                        std::int32_t d;
                        get(d, 0);
                        return d;
                    }
                    case 8: {
                        std::int64_t d;
                        get(d, 0);
                        return d;
                    }
                    default:
                        return 0;
                }
            }

            std::uint64_t get_uint() const {
                if (!is_uint()) {
                    return 0;
                }
                switch (size()) {
                    case 1: {
                        byte d;
                        get(d, 0);
                        return d;
                    }
                    case 2: {
                        std::uint16_t d;
                        get(d, 0);
                        return d;
                    }
                    case 4: {
                        std::uint32_t d;
                        get(d, 0);
                        return d;
                    }
                    case 8: {
                        std::uint64_t d;
                        get(d, 0);
                        return d;
                    }
                    default:
                        return 0;
                }
            }

            constexpr vtable_fn_t* get_vtable_ptr() const {
                if (!traits(Traits::is_polymorphic)) {
                    return nullptr;
                }
                std::uintptr_t ptr = 0;
                get(ptr);
                return std::bit_cast<vtable_fn_t*>(ptr);
            }

            constexpr bool set_vtable_ptr(vtable_fn_t* fn) {
                if (!traits(Traits::is_polymorphic)) {
                    return false;
                }
                return set(std::uintptr_t(fn));
            }
        };

        template <class T, class From, class... Other>
        bool assign(void* target, Reflect& from) {
            auto t = static_cast<T*>(target);
            if (from.visit<From>([&](auto&& f) {
                    *t = std::forward<decltype(f)>(f);
                })) {
                return true;
            }
            if constexpr (sizeof...(Other) != 0) {
                return assign<T, Other...>(target, from);
            }
            return false;
        }

        struct ArgVisitor {
           private:
            void* ctx;
            bool (*arg)(void* ctx, Reflect&, size_t idx);

            template <size_t idx, class... Refs>
            static bool visiting(std::tuple<Refs...>& tup, Reflect& r, size_t target) {
                if constexpr (idx < sizeof...(Refs)) {
                    if (idx == target) {
                        r.scan(std::forward<decltype(std::get<idx>(tup))>(std::get<idx>(tup)));
                        return true;
                    }
                    if constexpr (idx + 1 < sizeof...(Refs)) {
                        return visiting<idx + 1>(tup, r, target);
                    }
                }
                return false;
            }

            template <class... Refs>
            static bool visit(void* tup, Reflect& r, size_t idx) {
                if (!tup) {
                    return false;
                }
                auto& args = *static_cast<std::tuple<Refs...>*>(tup);
                return visiting<0>(args, r, idx);
            }

           public:
            template <class... Refs>
            constexpr ArgVisitor(std::tuple<Refs...>& refs)
                : ctx(&refs), arg(visit<Refs...>) {}

            bool get_arg(Reflect& r, size_t index) {
                if (!arg) {
                    return false;
                }
                return arg(ctx, r, index);
            }
        };

    }  // namespace reflect
}  // namespace futils

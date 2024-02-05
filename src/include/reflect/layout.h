/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <core/byte.h>
#include <binary/signint.h>
#include <binary/number.h>
#include <core/strlen.h>
#include <strutil/equal.h>
#include <tuple>

namespace futils::reflect {

#define REFLECT_LAYOUT_BUILDER(name)                             \
    struct name {                                                \
        static constexpr auto struct_name = #name;               \
        constexpr auto operator()(auto&& field) const noexcept { \
            return std::tuple {
#define REFLECT_LAYOUT_FIELD(name, type) field(#name, type{}),

#define REFLECT_LAYOUT_BUILDER_END() \
    int()                            \
    }                                \
    ;                                \
    }                                \
    }                                \
    ;

    template <class Builder, size_t alignment>
    struct Layout;

    namespace internal {
        constexpr auto count_from_builder(auto&& builder) {
            size_t i = 0;
            auto field1 = [&](auto, auto) {
                i++;
                return 0;
            };
            builder(field1);
            return i;
        }

        constexpr auto align_from_builder(auto&& builder) {
            size_t max_align = 1;
            auto field1 = [&](auto, auto val) {
                if (alignof(decltype(val)) > max_align) {
                    max_align = alignof(decltype(val));
                }
                return 0;
            };
            builder(field1);
            return max_align;
        }

        constexpr size_t diff(size_t base_align, size_t add_align, size_t cur_offset) {
            if (base_align == 1) {
                return 0;
            }
            auto align = add_align;
            if (add_align > base_align) {
                align = base_align;
            }
            auto delta = cur_offset % align;
            if (delta) {
                return align - delta;
            }
            return 0;
        }

        constexpr auto size_until_count_from_builder(auto&& builder, size_t count, size_t align) {
            size_t size_sum = 0;
            size_t i = 0;
            size_t prev_align = 0;
            auto field1 = [&](auto, auto val) {
                if (i < count) {
                    auto size = sizeof(val);
                    auto cur_align = alignof(decltype(val));
                    auto padding = diff(align, cur_align, size_sum);
                    size_sum += padding;
                    size_sum += size;
                    prev_align = cur_align;
                }
                i++;
                return 0;
            };
            builder(field1);
            return size_sum;
        }

        constexpr auto size_from_builder(auto&& builder, size_t align) {
            auto size = size_until_count_from_builder(builder, count_from_builder(builder), align);
            return size + (size % align);
        }

        constexpr auto size_at_i_from_builder(auto&& builder, size_t i) {
            size_t size = 0;
            size_t j = 0;
            auto field1 = [&](auto, auto val) {
                if (i == j) {
                    size = sizeof(val);
                }
                j++;
                return 0;
            };
            builder(field1);
            return size;
        }

        constexpr auto is_x_at_i_from_builder(auto&& builder, size_t i, auto&& is_x) {
            bool s = false;
            size_t j = 0;
            auto field1 = [&](auto, auto val) {
                if (i == j) {
                    s = is_x(val);
                }
                j++;
                return 0;
            };
            builder(field1);
            return s;
        }

        template <size_t i>
        constexpr auto type_of_builder_at_i(auto&& builder) {
            size_t j = 0;
            auto g = [&](auto, auto d) {
                return d;
            };
            auto tup = builder(g);
            return get<i>(tup);
        }

        constexpr auto name_at_i_from_builder(auto&& builder, size_t i) {
            size_t j = 0;
            const char* id = nullptr;
            auto field1 = [&](auto name, auto) {
                if (i == j) {
                    id = name;
                }
                j++;
                return 0;
            };
            builder(field1);
            return id;
        }

        template <class Builder>
        struct NameLookup {
            size_t index = -1;

            consteval NameLookup(const char* name) {
                if (!name) {
                    throw "unexpected nullptr";
                }
                Builder builder;
                size_t i = 0;
                auto field = [&](auto id, auto) {
                    if (strutil::equal(id, name)) {
                        if (index != -1) {
                            [](auto id) { throw "duplicated name referred"; }(id);
                        }
                        index = i;
                    }
                    i++;
                    return 0;
                };
                builder(field);
                if (index == -1) {
                    [](auto id) { throw "such name not found"; }(name);
                }
            }
        };

        template <class T>
        struct IsLayout : std::false_type {
        };

        template <class Builder, size_t alignment>
        struct IsLayout<Layout<Builder, alignment>> : std::true_type {};

    }  // namespace internal

    template <class Builder, size_t alignment = internal::align_from_builder(Builder{})>
    struct Layout {
        using builder_t = Builder;

        static constexpr auto align = alignment;
        static constexpr auto size = internal::size_from_builder(Builder{}, align);
        static constexpr auto field_count = internal::count_from_builder(Builder{});

        alignas(align) byte data[size]{};

        template <class T>
        constexpr T* cast() {
            if (sizeof(T) == size && alignof(T) == align) {
                return std::bit_cast<T*>(+data);
            }
            return nullptr;
        }

        template <class T>
        constexpr const T* cast() const {
            if (sizeof(T) == size && alignof(T) == align) {
                return std::bit_cast<const T*>(+data);
            }
            return nullptr;
        }

        static constexpr auto struct_name() {
            return Builder::struct_name;
        }

        template <size_t i>
        static constexpr auto type_at() {
            static_assert(i < field_count);
            return internal::type_of_builder_at_i<i>(Builder{});
        }

        template <size_t i>
        using type_at_t = decltype(type_at<i>());

        template <size_t i>
        using type_at_uint_t = binary::n_byte_uint_t<sizeof(type_at_t<i>)>;

        template <size_t i>
        constexpr auto get() const noexcept {
            static_assert(i < field_count);
            constexpr auto ofs = offset<i>();
            type_at_uint_t<i> val;
            binary::read_from(val, data + ofs, !binary::is_little());
            return type_at_t<i>(val);
        }

        template <size_t i>
        constexpr void set(type_at_t<i> val) noexcept {
            static_assert(i < field_count);
            constexpr auto ofs = offset<i>();
            binary::write_into(data + ofs, type_at_uint_t<i>(val), !binary::is_little());
        }

        template <internal::NameLookup<Builder> b>
        constexpr auto get() const noexcept {
            return get<b.index>();
        }

        template <internal::NameLookup<Builder> b>
        constexpr auto set(type_at_t<b.index> s) noexcept {
            return set<b.index>(s);
        }

        template <size_t i>
        constexpr auto direct(bool allow_non_aligned = false) noexcept {
            using T = decltype(get<i>());
            constexpr auto ofs = offset<i>();
            auto ptr = data + ofs;
            if (!allow_non_aligned && std::uintptr_t(ptr) % alignof(T)) {
                return (T*)nullptr;
            }
            return std::bit_cast<T*>(ptr);
        }

        template <size_t i>
        constexpr auto direct(bool allow_non_aligned = false) const noexcept {
            using T = decltype(get<i>());
            constexpr auto ofs = offset<i>();
            auto ptr = data + ofs;
            if (!allow_non_aligned && std::uintptr_t(ptr) % alignof(T)) {
                return (T*)nullptr;
            }
            return std::bit_cast<T*>(ptr);
        }

        template <size_t i>
        static constexpr auto offset() {
            static_assert(i < field_count);
            constexpr auto prev = internal::size_until_count_from_builder(Builder{}, i, align);
            constexpr auto then = internal::size_until_count_from_builder(Builder{}, i + 1, align);
            return prev + (then - prev - sizeof(type_at_t<i>));
        }

        template <size_t i>
        static constexpr auto nameof() {
            constexpr auto name = internal::name_at_i_from_builder(Builder{}, i);
            return name;
        }

       private:
        template <size_t i>
        constexpr auto call(auto&& fn, bool allow_non_aligned) {
            constexpr auto name = internal::name_at_i_from_builder(Builder{}, i);
            auto mem = direct<i>(allow_non_aligned);
            auto val = get<i>();
            fn(name, mem, val);
        }

        template <size_t... i>
        constexpr void call_each(std::index_sequence<i...>, auto&& fn, bool allow_non_aligned) {
            (..., call<i>(fn, allow_non_aligned));
        }

       public:
        template <class Fn>
        constexpr auto apply(Fn&& fn, bool allow_non_aligned = false) {
            call_each(std::make_index_sequence<field_count>{}, fn, allow_non_aligned);
        }
    };

    namespace test {
        REFLECT_LAYOUT_BUILDER(LTest)
        REFLECT_LAYOUT_FIELD(id, std::uint16_t)
        REFLECT_LAYOUT_FIELD(name, std::uint32_t)
        REFLECT_LAYOUT_BUILDER_END()

        static_assert(Layout<LTest>::align == 4 && Layout<LTest>::size == 8);

        constexpr bool check_name() {
            Layout<LTest> ltest;
            ltest.set<"id">(10);
            ltest.set<"name">(20);
            return ltest.get<"id">() == 10 &&
                   ltest.get<"name">() == 20;
        }
    }  // namespace test

}  // namespace futils::reflect

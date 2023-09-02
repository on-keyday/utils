/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <cstdint>
#include "../core/byte.h"

namespace utils {
    namespace binary {

        // make t unsigned integer type
        template <class T>
        constexpr auto uns(T t) {
            return std::make_unsigned_t<T>(t);
        }

        template <class T>
        constexpr auto indexed_mask() {
            return [](auto i) {
                return uns(T(1)) << (sizeof(T) * bit_per_byte - 1 - i);
            };
        }

        template <class T>
        constexpr bool is_separated(T v) {
            using U = std::make_unsigned_t<T>;
            U t = v;
            auto m = indexed_mask<U>();
            auto i = 0;
            for (; i < sizeof(T) * bit_per_byte; i++) {
                if (t & m(i)) {
                    break;
                }
            }
            for (; i < sizeof(T) * bit_per_byte; i++) {
                if (!(t & m(i))) {
                    break;
                }
            }
            for (; i < sizeof(T) * bit_per_byte; i++) {
                if (t & m(i)) {
                    return true;
                }
            }
            return false;
        }

        static_assert(!is_separated(0xff) &&
                      !is_separated(0) &&
                      is_separated(0xFD) &&
                      is_separated(0xF001));

        template <class T>
        constexpr bool is_flag_masks(auto... a) {
            return (... && !is_separated(a));
        }

        static_assert(is_flag_masks<std::uint8_t>(0x1, 0xFE));

        template <class T>
        constexpr byte mask_shift(T t) {
            using U = std::make_unsigned_t<T>;
            U v = U(t);
            v &= -v;
            byte n = 0;
            while (v != 0 && v != 1) {
                v >>= 1;
                n++;
            }
            return n;
        }

        template <class T>
        constexpr bool is_one_bit(T t) {
            using U = std::make_unsigned_t<T>;
            return (U(t) & -U(t)) == U(t);
        }

        template <size_t value>
        constexpr auto decide_int_type() {
            if constexpr (value <= 0xFF) {
                return std::uint8_t();
            }
            else if constexpr (value <= 0xFFFF) {
                return std::uint16_t();
            }
            else if constexpr (value <= 0xFFFFFFFF) {
                return std::uint32_t();
            }
            else {
                return std::uint64_t();
            }
        }

        template <size_t value>
        using value_max_uint_t = decltype(decide_int_type<value>());

        constexpr std::uint64_t n_bit_max(size_t n) {
            if (n == 0 || n > 64) {
                return 0;
            }
            constexpr auto n64_max = ~std::uint64_t(0);
            return n64_max >> (64 - n);
        }

        namespace internal {

            template <size_t s, size_t l>
            constexpr size_t log2i_impl(std::uint64_t n) {
                if constexpr (s + 1 == l) {
                    constexpr auto s_max = n_bit_max(s);
                    return n <= s_max ? s : l;
                }
                else {
                    constexpr auto m = (s + l) >> 1;
                    constexpr auto mid = n_bit_max(m);
                    return n < mid ? log2i_impl<s, m>(n) : log2i_impl<m, l>(n);
                }
            }

            constexpr auto test1 = log2i_impl<0, 64>(31);
            constexpr auto test2 = log2i_impl<0, 64>(8);
            constexpr auto test3 = log2i_impl<0, 64>(0);

            constexpr bool check_border() {
                for (auto i = 0; i <= 64; i++) {
                    auto n = n_bit_max(i);
                    if (log2i_impl<0, 64>(n) != i || log2i_impl<0, 64>(n + 1) == i) {
                        return false;
                    }
                }
                return true;
            }

            static_assert(check_border());

        }  // namespace internal

        constexpr size_t log2i(std::uint64_t n) {
            if (n == 1) {
                return 0;
            }
            else if (n == 2 || n == 3) {
                return 1;
            }
            return internal::log2i_impl<0, 64>(n);
        }

        template <class T, T... masks>
        struct Flags {
           private:
            using U = std::make_unsigned_t<T>;
            static_assert(is_flag_masks<U>(masks...));
            U flags = U(0);

            template <byte i, byte index, T m, T... o>
            static constexpr T mask_for_index() {
                if constexpr (i == index) {
                    return m;
                }
                else {
                    return mask_for_index<i + 1, index, o...>();
                }
            }

           public:
            template <byte i>
            static constexpr T get_mask() {
                return mask_for_index<0, i, masks...>();
            }

            template <byte i>
            static constexpr auto shift() {
                constexpr auto mask = get_mask<i>();
                return mask_shift(mask);
            }

            template <byte i>
            static constexpr auto limit() {
                constexpr auto mask = get_mask<i>();
                constexpr auto sh = mask_shift(mask);
                return uns(mask) >> sh;
            }

            template <byte i>
            constexpr auto set(auto val) {
                constexpr T mask = get_mask<i>();
                if constexpr (is_one_bit(mask)) {
                    if (val) {
                        flags |= uns(mask);
                    }
                    else {
                        flags &= ~uns(mask);
                    }
                }
                else {
                    constexpr auto sh = shift<i>();
                    constexpr auto lim = limit<i>();
                    if (uns(val) > lim) {
                        return false;
                    }
                    flags &= ~uns(mask);
                    flags |= (uns(T(val)) << sh);
                    return true;
                }
            }

            template <byte i>
            constexpr auto get() const {
                constexpr T mask = mask_for_index<0, i, masks...>();
                if constexpr (is_one_bit(mask)) {
                    return bool(flags & uns(mask));
                }
                else {
                    constexpr auto sh = shift<i>();
                    constexpr auto lim = limit<i>();
                    return value_max_uint_t<lim>((flags & uns(mask)) >> sh);
                }
            }

            constexpr const U& as_value() const {
                return flags;
            }

            constexpr U& as_value() {
                return flags;
            }
        };

        namespace test {
            constexpr bool check_flags() {
                Flags<std::uint8_t, 0xC0, 0x20> flags;
                return flags.set<0>(2) &&
                       !flags.get<1>() &&
                       flags.get<0>() == 2 &&
                       !flags.set<0>(4);
            }

            static_assert(check_flags());
        }  // namespace test

        namespace internal {

            template <class T, byte... split>
            struct Split {
                T masks[sizeof...(split)];

                constexpr auto init_masks() {
                    static_assert(sizeof(T) * bit_per_byte >= (... + split));
                    auto m = indexed_mask<T>();
                    auto t = uns(T());
                    byte sp = 0;
                    byte idx = 0;
                    auto p = [&](byte s) {
                        t = 0;
                        for (byte i = 0; i < s; i++) {
                            t |= m(sp + i);
                        }
                        masks[idx] = t;
                        idx++;
                        sp += s;
                    };
                    (..., p(split));
                }

                constexpr Split() {
                    init_masks();
                }
            };

            template <size_t i, class T, byte... s>
            constexpr T get(const Split<T, s...>& v) {
                return v.masks[i];
            }

            template <class T, class SpT, size_t... i>
            constexpr auto make_split_flags(std::index_sequence<i...>) {
                constexpr auto type = SpT{};
                return Flags<T, get<i>(type)...>{};
            }
        }  // namespace internal

        template <class T, byte... split>
        constexpr auto make_flags_value() {
            using SpT = internal::Split<T, split...>;
            return internal::make_split_flags<T, SpT>(std::make_index_sequence<sizeof...(split)>());
        }

        template <class T, byte... split>
        using flags_t = decltype(make_flags_value<T, split...>());

#define bits_flag_alias_method(flags, num, name)                                  \
    constexpr auto name() const noexcept {                                        \
        return flags.template get<num>();                                         \
    }                                                                             \
    constexpr auto set_##name(decltype(flags.template get<num>()) val) noexcept { \
        return flags.template set<num>(val);                                      \
    }                                                                             \
    static constexpr auto name##_max = decltype(flags)::template limit<num>();

#define bits_flag_alias_method_with_enum(flags, num, name, Enum)                  \
    constexpr auto name() const noexcept {                                        \
        return Enum(flags.template get<num>());                                   \
    }                                                                             \
    constexpr auto set_##name(Enum val) noexcept {                                \
        return flags.template set<num>(decltype(flags.template get<num>())(val)); \
    }                                                                             \
    static constexpr auto name##_max = decltype(flags)::limit<num>();

    }  // namespace binary
}  // namespace utils

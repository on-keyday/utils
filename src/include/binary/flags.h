/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <cstdint>
#include <core/byte.h>
#include <utility>
#include <cstddef>
#include <binary/signint.h>

namespace futils {
    namespace binary {

        template <class T>
        constexpr bool is_separated(T v) {
            using U = uns_t<T>;
            U t = uns(v);
            auto m = indexed_mask<U>();
            size_t i = 0;
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
            using U = uns_t<T>;
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
            using U = uns_t<T>;
            return (U(t) & -U(t)) == U(t);
        }

        template <class T, T... masks>
        struct Flags {
           private:
            using U = uns_t<T>;
            static_assert(is_flag_masks<U>(masks...));
            T flags = T(0);

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
            constexpr Flags() = default;

            constexpr Flags(T v)
                : flags(v) {}

            constexpr Flags(U u)
                requires(!std::is_same_v<T, U>)
                : flags(T(u)) {}

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
                        flags = static_cast<T>(uns(flags) | uns(mask));
                    }
                    else {
                        flags = static_cast<T>(uns(flags) & ~uns(mask));
                    }
                }
                else {
                    constexpr auto sh = shift<i>();
                    constexpr auto lim = limit<i>();
                    if (uns(val) > lim) {
                        return false;
                    }
                    flags = T((uns(flags) & ~uns(mask)) | (uns(T(val)) << sh));
                    return true;
                }
            }

            template <byte i>
            constexpr auto get() const {
                constexpr T mask = mask_for_index<0, i, masks...>();
                if constexpr (is_one_bit(mask)) {
                    return bool(uns(flags) & uns(mask));
                }
                else {
                    constexpr auto sh = shift<i>();
                    constexpr auto lim = limit<i>();
                    return value_max_uint_t<U, lim>((uns(flags) & uns(mask)) >> sh);
                }
            }

            constexpr const T& as_value() const {
                return flags;
            }

            constexpr T& as_value() {
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
                        masks[idx] = T(t);
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

#define bits_flag_alias_method(flags, num, name)                               \
    constexpr auto name() const noexcept {                                     \
        return flags.template get<num>();                                      \
    }                                                                          \
    constexpr auto name(decltype(flags.template get<num>()) val__) noexcept {  \
        return flags.template set<num>(val__);                                 \
    }                                                                          \
    static constexpr auto name##_max = decltype(flags)::template limit<num>(); \
    static constexpr auto name##_mask = decltype(flags)::template get_mask<num>();

#define bits_flag_alias_method_with_enum(flags, num, name, Enum)                    \
    constexpr auto name() const noexcept {                                          \
        return Enum(flags.template get<num>());                                     \
    }                                                                               \
    constexpr auto name(Enum val__) noexcept {                                      \
        return flags.template set<num>(decltype(flags.template get<num>())(val__)); \
    }                                                                               \
    static constexpr auto name##_max = decltype(flags)::limit<num>();               \
    static constexpr auto name##_mask = decltype(flags)::template get_mask<num>();

        namespace test {
#ifdef FUTILS_BINARY_SUPPORT_INT128
            constexpr bool check_128bit() {
                flags_t<uint128_t, 1, 127> val;
                val.set<0>(true);
                val.set<1>(32);
                return val.get<0>() &&
                       val.get<1>() == 32;
            }

            static_assert(check_128bit());
#endif
        }  // namespace test

    }  // namespace binary
}  // namespace futils

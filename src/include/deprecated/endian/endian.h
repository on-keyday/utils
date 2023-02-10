/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// endian - convert big endian and little endian
#pragma once

#if __has_include(<bit>)
#define UTILS_USE_STD_ENDIAN
#include <bit>
#endif

#include <type_traits>

namespace utils {
    namespace endian {
        enum class Endian {
            big = 0,
            little = 1,
            network = big,
            unknown = 2,
        };

        namespace internal {
            inline Endian native_impl() {
                int base = 1;
                const char* ptr = reinterpret_cast<const char*>(&base);
                if (ptr[0]) {
                    return Endian::little;
                }
                else {
                    return Endian::big;
                }
            }

            template <class T, class C>
            constexpr auto endinan_reqirement = (std::is_integral_v<T> || std::is_enum_v<T>)&&(std::is_integral_v<C> || std::is_enum_v<C>);

            template <class T, class C>
            constexpr T copy_as_is(C* in) {
                static_assert(endinan_reqirement<T, C>, "T and C must be integer");
                T result = T{};
                const char* inptr = reinterpret_cast<const char*>(in);
                char* outptr = reinterpret_cast<char*>(&result);
                for (auto i = 0; i < sizeof(T); i++) {
                    outptr[i] = inptr[i];
                }
                return result;
            }

            template <class T, class C>
            constexpr T reverse_endian(C* in) {
                static_assert(endinan_reqirement<T, C>,
                              "T and C must be integer or enum");
                T result = T{};
                const char* inptr = reinterpret_cast<const char*>(in);
                char* outptr = reinterpret_cast<char*>(&result);
                for (auto i = 0; i < sizeof(T); i++) {
                    outptr[sizeof(T) - 1 - i] = inptr[i];
                }
                return result;
            }
        }  // namespace internal
#ifdef UTILS_USE_STD_ENDIAN
        consteval Endian native() {
            return std::endian::native == std::endian::big ? Endian::big : std::endian::native == std::endian::little ? Endian::little
                                                                                                                      : Endian::unknown;
        }
#else
        inline Endian native() {
            return internal::native_impl();
        }
#endif

        // _us suffix means unsafe

        template <class T, class C>
        [[deprecated]] constexpr T to_big_us(C* c) {
            return native() == Endian::little ? internal::reverse_endian<T>(c) : internal::copy_as_is<T>(c);
        }

        template <class T, class C>
        [[deprecated]] constexpr T to_little_us(C* c) {
            return native() == Endian::big ? internal::reverse_endian<T>(c) : internal::copy_as_is<T>(c);
        }

        template <class T, class C>
        [[deprecated]] constexpr T to_network_us(C* c) {
            return to_big_us<T>(c);
        }

        template <class T>
        [[deprecated]] constexpr std::remove_cv_t<T> to_big(T* t) {
            return to_big_us<std::remove_cv_t<T>, T>(t);
        }

        template <class T>
        [[deprecated]] constexpr std::remove_cv_t<T> to_little(T* t) {
            return to_little_us<std::remove_cv_t<T>, T>(t);
        }

        template <class T>
        [[deprecated]] constexpr std::remove_cv_t<T> to_network(T* t) {
            return to_network_us<std::remove_cv_t<T>, T>(t);
        }

        // from_* and to_* are same, but to show semantics distinction, provided

        template <class T>
        [[deprecated]] constexpr std::remove_cv_t<T> from_big(T* t) {
            return to_big_us<std::remove_cv_t<T>, T>(t);
        }

        template <class T>
        [[deprecated]] constexpr std::remove_cv_t<T> from_little(T* t) {
            return to_little_us<std::remove_cv_t<T>, T>(t);
        }

        template <class T>
        [[deprecated]] constexpr std::remove_cv_t<T> from_network(T* t) {
            return to_network_us<std::remove_cv_t<T>, T>(t);
        }

    }  // namespace endian
}  // namespace utils

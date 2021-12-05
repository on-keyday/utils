/*license*/

// endian - convert big endian and little endian
#pragma once

#if __has_include(<bit>)
#define UTILS_USE_STD_ENDIAN
#include <bit>
#endif
namespace utils {

    enum class Endian {
        big,
        little,
        network = big,
        unknown,
    };
    namespace internal {
        Endian native_impl() {
            int base = 1;
            char* ptr = reinterpret_cast<char*>(&base);
            if (ptr[0]) {
                return Endian::little;
            }
            else {
                return Endian::big;
            }
        }
    }  // namespace internal
#ifdef UTILS_USE_STD_ENDIAN
    consteval Endian native() {
        return std::endian::native == std::endian::big ? Endian::big : std::endian::native == std::endian::little ? Endian::little
                                                                                                                  : Endian::unknown;
    }
#else
    Endian native() {
        static Endian native_ = internal::native_impl();
        return native_;
    }
#endif

    template <class T, class C>
    constexpr T reverse(C* in) {
        static_assert(std::is_integral_v<T>, "T must be integer");
        T result = 0;
        char* inptr = reinterpret_cast<char*>(in);
        char* outptr = reinterpret_cast<char*>(&result);
        for (auto i = 0; i < sizeof(T); i++) {
            outptr[sizeof(T) - 1 - i] = inptr[i];
        }
        return result;
    }

}  // namespace utils

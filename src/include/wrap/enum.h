/*license*/

// enum -  wrapper of enum
#pragma once

#include <utility>
#include <compare>

namespace utils {
    namespace wrap {

#define DEFINE_ENUM_FLAGOP(TYPE)                                                   \
    constexpr TYPE operator&(TYPE a, TYPE b) {                                     \
        using basety = std::underlying_type_t<TYPE>;                               \
        return static_cast<TYPE>(static_cast<basety>(a) & static_cast<basety>(b)); \
    }                                                                              \
    constexpr TYPE operator|(TYPE a, TYPE b) {                                     \
        using basety = std::underlying_type_t<TYPE>;                               \
        return static_cast<TYPE>(static_cast<basety>(a) | static_cast<basety>(b)); \
    }                                                                              \
    constexpr TYPE operator~(TYPE a) {                                             \
        using basety = std::underlying_type_t<TYPE>;                               \
        return static_cast<TYPE>(~static_cast<basety>(a));                         \
    }                                                                              \
    constexpr TYPE operator^(TYPE a, TYPE b) {                                     \
        using basety = std::underlying_type_t<TYPE>;                               \
        return static_cast<TYPE>(static_cast<basety>(a) ^ static_cast<basety>(b)); \
    }                                                                              \
    constexpr TYPE& operator&=(TYPE& a, TYPE b) {                                  \
        a = a & b;                                                                 \
        return a;                                                                  \
    }                                                                              \
    constexpr TYPE& operator|=(TYPE& a, TYPE b) {                                  \
        a = a | b;                                                                 \
        return a;                                                                  \
    }                                                                              \
    constexpr TYPE& operator^=(TYPE& a, TYPE b) {                                  \
        a = a ^ b;                                                                 \
        return a;                                                                  \
    }                                                                              \
    constexpr bool any(TYPE a) {                                                   \
        using basety = std::underlying_type_t<TYPE>;                               \
        return static_cast<basety>(a) != 0;                                        \
    }

#define DEFINE_ENUM_COMPAREOP(TYPE)                               \
    constexpr std::strong_ordering operator<=>(TYPE a, TYPE b) {  \
        using basety = std::underlying_type_t<TYPE>;              \
        return static_cast<basety>(a) <=> static_cast<basety>(b); \
    }

        template <class E, E trueval, E falseval, E defaultval = falseval>
        struct EnumWrap {
            E e;
            constexpr EnumWrap()
                : e(defaultval) {}
            constexpr EnumWrap(bool i)
                : e(i ? trueval : falseval) {}
            constexpr EnumWrap(E i)
                : e(i) {}
            constexpr operator bool() const {
                return e == trueval;
            }

            constexpr operator E() const {
                return e;
            }

            constexpr EnumWrap operator|(const EnumWrap& i) const {
                return e | i.e;
            }

            constexpr EnumWrap operator&(const EnumWrap& i) const {
                return e & i.e;
            }

            constexpr EnumWrap operator^(const EnumWrap& i) const {
                return e ^ i.e;
            }

            constexpr EnumWrap& operator|=(const EnumWrap& i) {
                e |= i.e;
                return *this;
            }

            constexpr EnumWrap& operator&=(const EnumWrap& i) {
                e &= i.e;
                return *this;
            }

            constexpr EnumWrap& operator^=(const EnumWrap& i) {
                e ^= i.e;
                return *this;
            }

            constexpr EnumWrap operator~() const {
                return ~e;
            }

            constexpr std::strong_ordering operator<=>(const EnumWrap& i) const {
                return e <=> i.e;
            }

            constexpr bool is(const EnumWrap& other) const {
                return e == other.e;
            }
        };

    }  // namespace wrap
}  // namespace utils

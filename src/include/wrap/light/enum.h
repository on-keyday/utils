/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// enum -  wrapper of enum
#pragma once

#include <utility>
#include <compare>

namespace futils {
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

#define BEGIN_ENUM_STRING_MSG(TYPE, FUNC) \
    constexpr const char* FUNC(TYPE e) {  \
        switch (e) {
#define BEGIN_ENUM_ERROR_MSG(TYPE) BEGIN_ENUM_STRING_MSG(TYPE, error_message)

#define ENUM_STRING_MSG(e, word) \
    case e:                      \
        return word;
#define ENUM_ERROR_MSG(e, word) ENUM_STRING_MSG(e, word)

#define END_ENUM_STRING_MSG(word) \
    default:                      \
        return word;              \
        }                         \
        }

#define END_ENUM_ERROR_MSG END_ENUM_STRING_MSG("unknown error")

    }  // namespace wrap
}  // namespace futils

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

    }  // namespace wrap
}  // namespace utils

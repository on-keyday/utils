/*license*/

// sfinae - SFINAE helper
#pragma once
#include <type_traits>

namespace utils {
    namespace helper {
#define SFINAE_HELPER_BASE(NAME, PARAM, ARG, EXPR)                            \
    struct NAME##_impl {                                                      \
        template PARAM static std::true_type has(decltype((EXPR), (void)0)*); \
        template PARAM static std::false_type has(...);                       \
    };                                                                        \
    template PARAM struct NAME : decltype(NAME##_impl::template has ARG(nullptr)) {}

#define DEFINE_SFINAE_T(NAME, EXPR) SFINAE_HELPER_BASE(NAME, <class T>, <T>, EXPR)
#define DEFINE_SFINAE_TU(NAME, EXPR) SFINAE_HELPER_BASE(NAME, <class T, class U>, <T, U>, EXPR)

    }  // namespace helper
}  // namespace utils

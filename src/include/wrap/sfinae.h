/*license*/

// sfinae - SFINAE helper
#pragma once
#include <type_traits>

namespace utils {
    namespace wrap {
#define sFINAE_HELPER_BASE(NAME, PARAM, ARG, EXPR)                            \
    struct NAME##_impl {                                                      \
        template PARAM static std::true_type has(decltype((EXPR), (void)0)*); \
        template PARAM static std::false_type has(...);                       \
    };                                                                        \
    template PARAM struct NAME : decltype(template NAME##_impl::has ARG(nullptr)) {}

    }  // namespace wrap
}  // namespace utils

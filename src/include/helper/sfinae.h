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

#define SFINAE_HELPER_COMMA ,

#define DEFINE_SFINAE_T(NAME, EXPR) SFINAE_HELPER_BASE(NAME, <class T>, <T>, EXPR)
#define DEFINE_SFINAE_TU(NAME, EXPR) SFINAE_HELPER_BASE(NAME, <class T SFINAE_HELPER_COMMA class U>, <T SFINAE_HELPER_COMMA U>, EXPR)

#define SFINAE_BLOCK_BEGIN_BASE(NAME, PARAM, ARG, EXPR, REMARG) \
    SFINAE_HELPER_BASE(NAME##_flag, PARAM, ARG, EXPR);          \
    template <REMARG, bool flag = NAME##_flag ARG::value>       \
    struct NAME {
#define SFINAE_BLOCK_ELSE_BASE(NAME, PARAM, ARG) \
    }                                            \
    ;                                            \
    template PARAM struct NAME ARG {
#define SFINAE_BLOCK_END_BASE() \
    }                           \
    ;

#define SFINAE_BLOCK_T_BEGIN(NAME, EXPR) SFINAE_BLOCK_BEGIN_BASE(NAME, <class T>, <T>, EXPR, class T)
#define SFINAE_BLOCK_T_ELSE(NAME) SFINAE_BLOCK_ELSE_BASE(NAME, <class T>, <T SFINAE_HELPER_COMMA false>)
#define SFINAE_BLOCK_T_END() SFINAE_BLOCK_END_BASE()

#define SFINAE_BLOCK_TU_BEGIN(NAME, EXPR) SFINAE_BLOCK_BEGIN_BASE(NAME, <class T SFINAE_HELPER_COMMA class U>, <T SFINAE_HELPER_COMMA U>, EXPR, class T SFINAE_HELPER_COMMA class U)
#define SFINAE_BLOCK_TU_ELSE(NAME) SFINAE_BLOCK_ELSE_BASE(NAME, <class T SFINAE_HELPER_COMMA class U>, <T SFINAE_HELPER_COMMA U SFINAE_HELPER_COMMA false>)
#define SFINAE_BLOCK_TU_END() SFINAE_BLOCK_END_BASE()
    }  // namespace helper
}  // namespace utils

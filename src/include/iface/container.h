/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// container - container like interface
#pragma once
#include "base_iface.h"

namespace utils {
    namespace iface {
        // hpack dymap implements
        // begin() -> Ranges
        // end() -> Ranges
        // push_front(T) -> Queue
        // size() -> Sized
        // pop_back() -> Queue
        // hpack string implements
        // == -> ?

        template <class B, class E, class Box>
        struct Range : Box {
           private:
            MAKE_FN(begin, B)
            MAKE_FN(end, E)

           public:
            DEFAULT_METHODS(Range)

            template <class T>
            Range(T&& t)
                : APPLY2_FN(begin),
                  APPLY2_FN(end),
                  Box(std::forward<T>(t)) {}

            B begin(){
                DEFAULT_CALL(begin, select_traits<B>())}

            E end() {
                DEFAULT_CALL(end, select_traits<E>())
            }
        };

        template <class U, class Box>
        struct Queue : Box {
           private:
            MAKE_FN_VOID(push_front, U)
            MAKE_FN_VOID(pop_back)

           public:
            template <class T>
            Queue(T&& t)
                : APPLY2_FN(push_front, U),
                  APPLY2_FN(pop_back),
                  Box(std::forward<T>(t)) {}

            void push_front(U t) {
                DEFAULT_CALL(push_front, (void)0, std::forward<U>(t))
            }

            void pop_back() {
                DEFAULT_CALL(pop_back, (void)0)
            }
        };

        template <class T, class B, class E, class Box>
        using HpackDyntableBase = Queue<T, Sized<Range<B, E, Box>>>;
        namespace internal {
            struct HpackDefHelper {
                template <class Box, class T>
                using iterator = ForwardIterator<Box, T&>;

                template <class T>
                using T_ = std::remove_cvref_t<T>;

                template <class T, class ContBox, class ItBox>
                using type = HpackDyntableBase<
                    T, iterator<ItBox, T_<T>>,
                    iterator<ItBox, T_<T>>, ContBox>;
            };

        }  // namespace internal

        template <class T, class ContBox, class ItBox = ContBox>
        using HpackQueue = internal::HpackDefHelper::type<T, ContBox, ItBox>;

    }  // namespace iface
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// priority_ext - extension of std::priority_quque
#pragma once
#include <queue>
#include "../wrap/lite/queue.h"
#include "channel.h"

namespace utils {
    namespace thread {
        template <class T, class Container = wrap::queue<T>, class Compare = std::less<>>
        struct WithRawContainer : std::priority_queue<T, Container, Compare> {
            using std::priority_queue<T, Container, Compare>::c;
            Container& container() {
                return c;
            }
        };

        struct DualModeHandler {
            bool priority_mode = false;

            template <class T, class Container, class Compare>
            bool remove(WithRawContainer<T, Container, Compare>& que, ChanDisposePolicy policy) {
                if (priority_mode) {
                    return false;
                }
                else {
                    if (policy == ChanDisposePolicy::dispose_front) {
                        que.container().pop_front();
                    }
                    else if (policy == ChanDisposePolicy::dispose_back) {
                        que.container().pop_back();
                    }
                    else {
                        return false;
                    }
                    return true;
                }
            }

            template <class T, class Container, class Compare>
            void push(WithRawContainer<T, Container, Compare>& que, T&& t) {
                if (priority_mode) {
                    que.push(std::move(t));
                }
                else {
                    que.container().push_back(std::move());
                }
            }

            template <class T, class Container, class Compare>
            void pop(WithRawContainer<T, Container, Compare>& que, T& t) {
                if (priority_mode) {
                    t = std::move(const_cast<T>(que.top()));
                    que.pop();
                }
                else {
                    t = std::move(que.container().front());
                    que.container().pop_front();
                }
            }
        };
    }  // namespace thread
}  // namespace utils

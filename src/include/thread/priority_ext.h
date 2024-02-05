/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// priority_ext - extension of std::priority_quque
#pragma once
#include <queue>
#include "../wrap/light/queue.h"
#include "channel.h"
#include "../strutil/compare_type.h"

namespace futils {
    namespace thread {
        template <class T, class Container = wrap::queue<T>, class Compare = std::less<>>
        struct WithRawContainer : std::priority_queue<T, Container, Compare> {
            using std::priority_queue<T, Container, Compare>::c;
            using std::priority_queue<T, Container, Compare>::comp;
            Container& container() {
                return c;
            }

            Compare& compare() {
                return comp;
            }
        };

        template <class PriorityChanger = decltype(strutil::no_check())>
        struct DualModeHandler {
           private:
            bool prev_ = false;

           public:
            bool priority_mode = false;
            size_t push_count = 0;
            PriorityChanger changer;

            template <class T, class Container, class Compare>
            void check_make_heap(WithRawContainer<T, Container, Compare>& que) {
                if (prev_ != priority_mode && que.size()) {
                    auto& cont = que.container();
                    std::make_heap(cont.begin(), cont.end(), que.compare());
                }
            }

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
                push_count++;
                if (push_count > 100) {
                    changer(que);
                    push_count = 0;
                }
                if (priority_mode) {
                    check_make_heap(que);
                    que.push(std::move(t));
                }
                else {
                    que.container().push_back(std::move(t));
                }
                prev_ = priority_mode;
            }

            template <class T, class Container, class Compare>
            void pop(WithRawContainer<T, Container, Compare>& que, T& t) {
                if (priority_mode) {
                    check_make_heap(que);
                    t = std::move(const_cast<T&>(que.top()));
                    que.pop();
                }
                else {
                    t = std::move(que.container().front());
                    que.container().pop_front();
                }
                prev_ = priority_mode;
            }
        };
    }  // namespace thread
}  // namespace futils

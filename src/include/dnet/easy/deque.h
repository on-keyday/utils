/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// deque - deque
#pragma once
#include <deque>
#include "../dll/allocator.h"
#include "../error.h"

namespace utils {
    namespace dnet {
        namespace easy {
            template <class T>
            struct Deque {
               private:
                std::deque<T, glheap_allocator<T> > que;

               public:
                error::Error push_back(auto&& t) {
                    que.push_back(std::forward<decltype(t)>(t));
                    return error::none;
                }

                auto begin() {
                    return que.begin();
                }

                auto end() {
                    return que.end();
                }

                auto data() {
                    return que.data();
                }

                auto data() const {
                    return que.data();
                }

                size_t size() const {
                    return que.size();
                }

                decltype(auto) operator[](size_t i) {
                    return que[i];
                }

                decltype(auto) operator[](size_t i) const {
                    return que[i];
                }

                auto empty() const {
                    return que.empty();
                }

                void clear() {
                    que.clear();
                }

                void pop_front() {
                    que.pop_front();
                }

                void pop_back() {
                    que.pop_back();
                }
            };
        }  // namespace easy
    }      // namespace dnet
}  // namespace utils
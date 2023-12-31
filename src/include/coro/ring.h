/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../helper/defer.h"
#include <cassert>

namespace utils {
    namespace coro {

        template <class T>
        struct RingQue {
           private:
            size_t que_capacity = 0;
            size_t top = 0;
            size_t end = 0;
            size_t stored_size = 0;
            T* que = nullptr;
            bool block_by_pop = false;

            constexpr void incr_top() {
                top++;
                if (top == que_capacity) {
                    top = 0;
                }
            }

            constexpr void incr_end() {
                end++;
                if (end == que_capacity) {
                    end = 0;
                }
                if (top == end) {
                    block_by_pop = true;
                }
            }

           public:
            constexpr T* set_que(T* place, size_t size) {
                auto prev = que;
                if (!place) {
                    size = 0;
                }
                que = place;
                que_capacity = size;
                stored_size = 0;
                top = 0;
                end = 0;
                block_by_pop = false;
                return prev;
            }

            constexpr bool push(T&& p) {
                if (block()) {
                    return false;
                }
                que[end] = std::move(p);
                incr_end();
                stored_size++;
                return true;
            }

            constexpr bool block() const {
                return que_capacity == 0 || top == end && block_by_pop;
            }

            constexpr bool empty() const {
                return que_capacity == 0 || top == end && !block_by_pop;
            }

            constexpr T pop() {
                if (empty()) {
                    return {};
                }
                auto got = std::exchange(que[top], T{});
                incr_top();
                block_by_pop = false;
                stored_size--;
                return got;
            }

            constexpr size_t steal(T* stolen, size_t l) {
                if (!stolen) {
                    return 0;
                }
                size_t i = 0;
                while (i < l && !empty()) {
                    stolen[i] = std::exchange(que[top], nullptr);
                    incr_top();
                    block_by_pop = false;
                    i++;
                }
                return i;
            }

            constexpr size_t capacity() const {
                return que_capacity;
            }

            constexpr size_t size() const {
                return stored_size;
            }

            constexpr size_t remain() const {
                return capacity() - size();
            }
        };

        namespace test {
            struct C {};
            struct L {
                static constexpr int c = 50;
                static constexpr int d = 40;
                static constexpr int b = 20;
                RingQue<C*> rg;
                C list[c]{};
                C* node[c]{};
                constexpr L() {
                    rg.set_que(node, b);
                    for (auto& n : list) {
                        rg.push(&n);
                    }
                    for (auto i = 0; i < c - d; i++) {
                        rg.pop();
                    }
                }
            };

            constexpr L check_clist{};

        }  // namespace test

        template <class Lock, class T>
        struct LockedRingQue {
           private:
            Lock l;
            RingQue<T> list;

            auto lock() {
                l.lock();
                return helper::defer([&] {
                    l.unlock();
                });
            }

           public:
            constexpr void set_que(T* place, size_t size) {
                const auto l = lock();
                list.set_que(place, size);
            }

            constexpr T pop() {
                const auto l = lock();
                return list.pop();
            }

            constexpr bool push(T&& c) {
                const auto l = lock();
                return list.push(std::move(c));
            }

            constexpr bool block() {
                const auto l = lock();
                return list.block();
            }

            constexpr bool empty() {
                const auto l = lock();
                return list.empty();
            }

            constexpr size_t steal(T* q, size_t s) {
                const auto l = lock();
                return list.steal(q, s);
            }
        };
    }  // namespace coro
}  // namespace utils

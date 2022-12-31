/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// vector - easy vector
#pragma once
#include "../dll/allocator.h"
#include <memory>
#include "../../helper/defer.h"
#include <initializer_list>
#include <vector>
#include "../error.h"

namespace utils {
    namespace dnet {
        namespace easy {
            /*template <class T, class Alloc = glheap_allocator<T>>
            struct vector {
               private:
                T* head = nullptr;
                size_t len = 0;
                size_t cap = 0;
                Alloc a{};
                static constexpr std::allocator_traits<Alloc> al{};

                constexpr void move(vector&& in) {
                    head = std::exchange(in.head, nullptr);
                    len = std::exchange(in.len, 0);
                    cap = std::exchange(in.cap, 0);
                    a = std::exchange(in.a, {});
                }

                bool do_copy(const vector& in) {
                    a = in.a;
                    if (!in.head) {
                        len = in.len;
                        cap = in.cap;
                        return true;
                    }
                    auto ptr = al.allocate(a, in.cap);
                    if (!ptr) {
                        return false;  // allocation failure
                    }
                    size_t i = 0;
                    auto r = helper::defer([&] {
                        for (auto k = 0; k < i - 1; k++) {
                            al.destroy(a, ptr[k]);
                        }
                        al.deallocate(a, ptr, in.cap);
                    });
                    for (; i < in.len; i++) {
                        al.construct(a, ptr[i], std::as_const(in.head[i]));
                    }
                    head = ptr;
                    len = in.len;
                    cap = in.cap;
                    r.cancel();
                    return true;
                }

               public:
                ~vector() {
                    for (size_t i = 0; i < len; i++) {
                        al.destroy(a, head[i]);
                    }
                    al.deallocate(head, cap);
                }

                constexpr vector() = default;

                vector(size_t cap) {
                    reserve(cap);
                }

                vector(std::initializer_list<T> elm) {
                    if (resize_impl(elm.size(), false)) {
                        for (size_t i = 0; i < elm.size(); i++) {
                            al.construct(a, head[i], std::move(elm.begin()[i]));
                        }
                    }
                }

                constexpr vector(vector&& in) {
                    move(std::move(in));
                }

                constexpr vector& operator=(vector&& in) noexcept {
                    if (this == &in) {
                        return *this;
                    }
                    this->~vector();
                    move(std::move(in));
                    return *this;
                }

                bool copy_from(const vector& in) {
                    if (this == &in) {
                        return true;
                    }
                    this->~vector();
                    return do_copy(in);
                }

                bool reserve(size_t c) {
                    if (cap >= c) {
                        return true;
                    }
                    auto ptr = al.allocate(a, c);
                    if (!ptr) {
                        return false;
                    }
                    for (size_t i = 0; i < len; i++) {
                        al.construct(a, ptr[i], std::move(head[i]));
                        al.destroy(a, head[i]);
                    }
                    al.deallocate(head);
                    head = ptr;
                    cap = c;
                    return true;
                }

               private:
                bool resize_impl(size_t l, bool do_construct) {
                    if (l == len) {
                        return true;
                    }
                    if (l < len) {
                        for (auto i = len - l; i < len; i++) {
                            al.destory(a, head[i]);
                        }
                        len = l;
                        return true;
                    }
                    if (l > cap) {
                        if (!reserve(cap * 2)) {
                            return false;
                        }
                    }
                    if (do_construct) {
                        for (auto i = len; i < l; i++) {
                            al.construct(a, head[i]);
                        }
                    }
                    len = l;
                    return true;
                }

               public:
                bool resize(size_t l) {
                    return resize_impl(l, true);
                }

                bool push_back(T&& t) {
                    if (!resize_impl(len + 1, false)) {
                        return false;
                    }
                    al.construct(a, head[len - 1], std::move(t));
                    return true;
                }

                T& operator[](size_t i) {
                    if (i >= len) {
                        return *(T*)nullptr;  // oops
                    }
                    return head[i];
                }

                const T& operator[](size_t i) const {
                    if (i >= len) {
                        return *(T*)nullptr;  // oops
                    }
                    return head[i];
                }

                T* begin() {
                    return head;
                }

                T* end() {
                    return head + len;
                }

                const T* begin() const {
                    return head;
                }

                const T* end() const {
                    return head + len;
                }
            };*/

            template <class T>
            struct Vec {
               private:
                std::vector<T, glheap_allocator<T>> vec;

               public:
                error::Error push_back(auto&& t) {
                    vec.push_back(std::forward<decltype(t)>(t));
                    return error::none;
                }

                auto begin() {
                    return vec.begin();
                }

                auto end() {
                    return vec.end();
                }

                auto begin() const {
                    return vec.begin();
                }

                auto end() const {
                    return vec.end();
                }

                auto data() {
                    return vec.data();
                }

                auto data() const {
                    return vec.data();
                }

                size_t size() const {
                    return vec.size();
                }

                decltype(auto) operator[](size_t i) {
                    return vec[i];
                }

                decltype(auto) operator[](size_t i) const {
                    return vec[i];
                }

                auto empty() const {
                    return vec.empty();
                }

                void clear() {
                    vec.clear();
                }
            };
        }  // namespace easy
    }      // namespace dnet
}  // namespace utils

/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// que - queue
#pragma once
#include "alloc.h"
#include <mutex>
#include <cassert>

namespace utils {
    namespace quic {
        namespace mem {
            template <class T>
            struct Que {
                T* vec = nullptr;
                tsize cap = 0;
                tsize len = 0;
                allocate::Alloc* a = nullptr;
                std::mutex m;

                auto lock_callback(auto&& callback) {
                    std::scoped_lock lock{m};
                    return callback();
                }

                bool init_nlock(tsize s) {
                    if (vec) {
                        return true;
                    }
                    if (!a) {
                        return false;
                    }
                    vec = a->allocate_vec<T>(s);
                    if (!vec) {
                        return false;
                    }
                    len = 0;
                    cap = s;
                    return true;
                }

                bool init(tsize s) {
                    return lock_callback([&]() { return init_nlock(s); });
                }

                bool en_q_nlock(T&& q) {
                    if (!vec) {
                        return false;
                    }
                    if (len + 1 < cap) {
                        vec[len] = std::move(q);
                        len++;
                        return true;
                    }
                    auto new_cap = cap * 2;
                    auto ptr = a->resize_vec(vec, cap, new_cap);
                    if (!ptr) {
                        return false;
                    }
                    vec = ptr;
                    cap = new_cap;
                    vec[len] = std::move(q);
                    len++;
                    return true;
                }

                bool en_q(T&& q) {
                    return lock_callback([&]() { return en_q_nlock(std::move(q)); });
                }

                bool de_q_nlock(T& q) {
                    if (len == 0) {
                        return false;
                    }
                    q = std::move(vec[0]);
                    for (auto i = 0; i < len; i++) {
                        vec[i] = vec[i + 1];
                        vec[i + 1] = T{};
                    }
                    len--;
                    return true;
                }

                bool de_q(T& q) {
                    return lock_callback([&]() { return de_q_nlock(q); });
                }

                tsize rem_q_nblock(const T& cmp) {
                    tsize rem = 0;
                    for (tsize i = 0; i < len - rem; i++) {
                        if (vec[i] == cmp) {
                            for (tsize k = i + 1; k < len - rem; k++) {
                                vec[k - 1] = std::move(vec[k]);
                            }
                            rem++;
                        }
                    }
                    return rem;
                }

                tsize rem_q(const T& cmp) {
                    return lock_callback([&]() { return rem_q_nblock(cmp); });
                }

                void destruct_nlock(auto&& callback) {
                    for (tsize i = 0; i < len; i++) {
                        callback(vec[i]);
                    }
                    a->deallocate_vec(vec, cap);
                    vec = nullptr;
                    cap = 0;
                    len = 0;
                    a = nullptr;
                }

                void destruct(auto&& callback) {
                    lock_callback([&]() { destruct_nlock(callback); });
                }
            };

            template <class T>
            struct LinkNode {
                T value;
                LinkNode* next;
            };

            template <class T>
            struct LinkQue {
                allocate::Alloc* a;

               private:
                // lock
                std::recursive_mutex m;

                LinkNode<T>* first = nullptr;
                LinkNode<T>* last = nullptr;

                // allocated stcok
                LinkNode<T>* stock_begin = nullptr;
                LinkNode<T>* stock_end = nullptr;

                static void add_node_range(LinkNode<T>*& root, LinkNode<T>*& rootend, LinkNode<T>* f, LinkNode<T>* e) {
                    if (!root) {
                        assert(rootend == nullptr);
                        root = f;
                        rootend = e;
                    }
                    else {
                        assert(rootend && rootend->next == nullptr);
                        rootend->next = f;
                        rootend = e;
                    }
                }

               public:
                auto lock_callback(auto&& callback) {
                    std::scoped_lock lock{m};
                    return callback();
                }

                bool en_q_nlock(T&& q) {
                    LinkNode<T>* set = nullptr;
                    // get from stack
                    if (stock_begin) {
                        set = stock_begin;
                        if (stock_begin == stock_end) {
                            assert(stock_begin->next == nullptr);
                            stock_end = nullptr;
                        }
                        stock_begin = stock_begin->next;
                        set->next = nullptr;
                    }
                    if (!set) {
                        // allocate new LinkNode
                        set = a ? a->allocate<LinkNode<T>>() : nullptr;
                        if (!set) {
                            return false;
                        }
                        set->next = nullptr;
                    }
                    set->value = std::move(q);
                    add_node_range(first, last, set, set);
                    return true;
                }

                bool en_q(T&& q) {
                    std::scoped_lock l{m};
                    return en_q_nlock(std::move(q));
                }

                bool de_q_nlock(T& q) {
                    if (!first) {
                        return false;
                    }
                    auto get = first;
                    if (first == last) {
                        assert(first->next == nullptr);
                        last = nullptr;
                    }
                    first = first->next;
                    get->next = nullptr;
                    q = std::move(get->value);
                    get->value = T{};
                    add_node_range(stock_begin, stock_end, get, get);
                    return true;
                }

                bool de_q(T& q) {
                    std::scoped_lock l{m};
                    return de_q_nlock(q);
                }

                bool init_nlock(tsize reserve) {
                    if (!a) {
                        return false;
                    }
                    if (first || stock_begin) {
                        return true;
                    }
                    for (auto i = 0; i < reserve; i++) {
                        auto n = a->allocate<LinkNode<T>>();
                        if (!n) {
                            return i != 0;
                        }
                        n->next = nullptr;
                        add_node_range(stock_begin, stock_end, n, n);
                    }
                    return true;
                }

                bool init(tsize reserve) {
                    std::scoped_lock l{m};
                    return init_nlock(reserve);
                }

                tsize rem_q_nlock(auto&& should_remove) {
                    LinkNode<T>* prev = nullptr;
                    tsize count = 0;
                    for (LinkNode<T>* s = first; s;) {
                        if (should_remove(s->value)) {
                            LinkNode<T>* next = s->next;
                            s->next = nullptr;
                            if (prev) {
                                assert(s != first);
                                if (s == last) {
                                    assert(next == nullptr);
                                    last = nullptr;
                                }
                                prev->next = next;
                            }
                            else {
                                assert(s == first);
                                if (s == last) {
                                    assert(next == nullptr);
                                    last = nullptr;
                                }
                                first = next;
                            }
                            s->value = {};
                            add_node_range(stock_begin, stock_end, s, s);
                            s = next;
                            count++;
                        }
                        else {
                            prev = s;
                            s = s->next;
                        }
                    }
                    return count;
                }

                tsize rem_q(auto&& should_remove) {
                    std::scoped_lock l{m};
                    return rem_q_nlock(should_remove);
                }

               private:
                static tsize steal_(std::mutex* lock, LinkNode<T>*& root, LinkNode<T>*& rootend, tsize max, auto&& callback) {
                    if (!root) {
                        return 0;
                    }
                    if (lock) {
                        lock->lock();
                    }
                    tsize count = 1;
                    LinkNode<T>* start = root;
                    LinkNode<T>* goal = root;
                    for (; count < max && goal->next; goal = goal->next) {
                        count++;
                    }
                    if (goal == rootend) {
                        assert(goal->next == nullptr);
                        rootend = nullptr;
                    }
                    root = goal->next;
                    goal->next = nullptr;
                    if (lock) {
                        lock->unlock();
                    }
                    callback(start, goal);
                    return count;
                }

               public:
                tsize steal(LinkQue<T>& by, tsize max, bool lock) {
                    if (a != by.a) {
                        return 0;
                    }
                    return steal_(
                        lock ? &m : nullptr,
                        first, last, max,
                        [&](LinkNode<T>* b, LinkNode<T>* e) {
                            std::scoped_lock l{by.m};
                            add_node_range(by.first, by.last, b, e);
                        });
                }

                tsize steal_stock(LinkQue<T>& by, tsize max, bool lock) {
                    if (a != by.a) {
                        return 0;
                    }
                    return steal_(
                        lock ? &m : nullptr,
                        stock_begin, stock_end, max,
                        [&](LinkNode<T>* b, LinkNode<T>* e) {
                            std::scoped_lock l{by.m};
                            add_node_range(by.stock_begin, by.stock_end, b, e);
                        });
                }

                void free_stock(bool lock) {
                    if (!stock_begin) {
                        return;
                    }
                    if (lock) {
                        m.lock();
                    }
                    auto stock = stock_begin;
                    stock_begin = nullptr;
                    stock_end = nullptr;
                    if (lock) {
                        m.unlock();
                    }
                    for (LinkNode<T>* s = stock; s;) {
                        auto n = s->next;
                        s->next = nullptr;
                        a->deallocate(s);
                        s = n;
                    }
                }

                void destruct(auto&& callback) {
                    std::scoped_lock lock{m};
                    if (!a) {
                        return;
                    }
                    free_stock(false);
                    for (LinkNode<T>* s = first; s;) {
                        auto n = s->next;
                        s->next = nullptr;
                        callback(s->value);
                        a->deallocate(s);
                        s = n;
                    }
                    a = nullptr;
                }
            };
        }  // namespace mem
    }      // namespace quic
}  // namespace utils

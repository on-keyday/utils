/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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
            struct EmptyLock {
                void lock() {}
                bool try_lock() {
                    return true;
                }
                void unlock() {}
            };

            template <class T, class TypeConfig = std::recursive_mutex>
            struct VecQue {
                T* vec = nullptr;
                tsize cap = 0;
                tsize len = 0;
                allocate::Alloc* a = nullptr;

                TypeConfig m;

                auto lock_callback(auto&& callback) {
                    std::scoped_lock l{m};
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
            void add_node_range(LinkNode<T>*& root, LinkNode<T>*& rootend, LinkNode<T>* f, LinkNode<T>* e) {
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

            template <class T, class TypeConfig>
            struct StockNode {
                LinkNode<T>* stock_begin = nullptr;
                LinkNode<T>* stock_end = nullptr;
                allocate::Alloc* a = nullptr;
                TypeConfig locker;

                void lock() {
                    locker.lock();
                }

                void unlock() {
                    locker.unlock();
                }

                allocate::Alloc* get_alloc() {
                    return a;
                }

                LinkNode<T>** begin() {
                    return &stock_begin;
                }

                LinkNode<T>** end() {
                    return &stock_end;
                }

                LinkNode<T>* get() {
                    if (stock_begin) {
                        auto set = stock_begin;
                        if (stock_begin == stock_end) {
                            assert(stock_begin->next == nullptr);
                            stock_end = nullptr;
                        }
                        stock_begin = stock_begin->next;
                        set->next = nullptr;
                        return set;
                    }
                    return nullptr;
                }

                void set_range(LinkNode<T>* begin, LinkNode<T>* end) {
                    add_node_range(stock_begin, stock_end, begin, end);
                }

                void set(LinkNode<T>* stock) {
                    set_range(stock, stock);
                }

                bool empty() const {
                    return stock_begin == nullptr;
                }
            };

            template <class T, class TypeConfig>
            struct SharedStock {
                StockNode<T, TypeConfig>* shared;

                allocate::Alloc* get_alloc() {
                    return shared->get_alloc();
                }

                LinkNode<T>** begin() {
                    return nullptr;
                }

                LinkNode<T>** end() {
                    return nullptr;
                }

                LinkNode<T>* get() {
                    return shared->get();
                }

                void set_range(LinkNode<T>* begin, LinkNode<T>* end) {
                    return shared->set_range(begin, end);
                }

                void set(LinkNode<T>* stock) {
                    set_range(stock, stock);
                }

                bool empty() const {
                    return shared->empty();
                }

                void lock() {
                    shared->lock();
                }

                void unlock() {
                    shared->unlock();
                }
            };

            template <class T>
            struct linkq_iterator {
               private:
                LinkNode<T>* node = nullptr;
                template <class T_, class Stock>
                friend struct LinkQue;
                constexpr linkq_iterator(LinkNode<T>* q)
                    : node(q) {}

               public:
                constexpr linkq_iterator() = default;
                T& operator*() {
                    return node->value;
                }

                linkq_iterator& operator++() {
                    node = node->next;
                    return *this;
                }

                linkq_iterator operator++(int) {
                    auto n = node;
                    node = node->next;
                    return linkq_iterator(n);
                }

                constexpr bool operator==(const linkq_iterator& it) const {
                    return node == it.node;
                }
            };

            template <class T, class Stock = StockNode<T, std::recursive_mutex>>
            struct LinkQue {
                // allocated stcok and lock
                Stock stock;

               private:
                LinkNode<T>* first = nullptr;
                LinkNode<T>* last = nullptr;

               public:
                auto lock_callback(auto&& callback) {
                    std::scoped_lock l{stock};
                    return callback();
                }

                bool en_q_node_nlock(LinkNode<T>* set) {
                    if (!set) {
                        return false;
                    }
                    add_node_range(first, last, set, set);
                    return true;
                }

                bool en_q_node(LinkNode<T>* set) {
                    std::scoped_lock l{stock};
                    return en_q_node_nlock(set);
                }

                bool en_q_nlock(T&& q) {
                    LinkNode<T>* set = nullptr;
                    // get from stack
                    set = stock.get();
                    if (!set) {
                        allocate::Alloc* al = stock.get_alloc();
                        // allocate new LinkNode
                        set = al ? al->allocate<LinkNode<T>>() : nullptr;
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
                    std::scoped_lock l{stock};
                    return en_q_nlock(std::move(q));
                }

                LinkNode<T>* de_q_node_nlock() {
                    if (!first) {
                        return nullptr;
                    }
                    auto get = first;
                    if (first == last) {
                        assert(first->next == nullptr);
                        last = nullptr;
                    }
                    first = first->next;
                    get->next = nullptr;
                    return get;
                }

                LinkNode<T>* de_q_node() {
                    std::scoped_lock l{stock};
                    return de_q_node_nlock();
                }

                bool de_q_nlock(T& q) {
                    LinkNode<T>* get = de_q_node_nlock();
                    if (!get) {
                        return false;
                    }
                    q = std::move(get->value);
                    get->value = T{};
                    stock.set(get);
                    return true;
                }

                bool de_q(T& q) {
                    std::scoped_lock l{stock};
                    return de_q_nlock(q);
                }

                bool init_nlock(tsize reserve) {
                    allocate::Alloc* al = stock.get_alloc();
                    if (!al) {
                        return false;
                    }
                    if (first || !stock.empty()) {
                        return true;
                    }
                    for (auto i = 0; i < reserve; i++) {
                        auto n = al->allocate<LinkNode<T>>();
                        if (!n) {
                            return i != 0;
                        }
                        n->next = nullptr;
                        stock.set(n);
                    }
                    return true;
                }

                bool init(tsize reserve) {
                    std::scoped_lock l{stock};
                    return init_nlock(reserve);
                }

                void for_each_nlock(auto&& callback) {
                    for (auto s = first; s; s = s->next) {
                        if (!callback(s->value)) {
                            return;
                        }
                    }
                }

                void for_each(auto&& callback) {
                    std::scoped_lock l{stock};
                    for_each_nlock(callback);
                }

                tsize rem_q_node_nlock(auto&& should_remove) {
                    LinkNode<T>* prev = nullptr;
                    tsize count = 0;
                    for (LinkNode<T>* s = first; s;) {
                        if (should_remove(s)) {
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
                            stock.set(s);
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

                tsize rem_q_nlock(auto&& should_remove) {
                    return rem_q_node_nlock([&](LinkNode<T>* node) {
                        return should_remove(node->value);
                    });
                }

                tsize rem_q(auto&& should_remove) {
                    std::scoped_lock l{stock};
                    return rem_q_nlock(should_remove);
                }

               private:
                template <class L>
                static tsize steal_(L* l, LinkNode<T>*& root, LinkNode<T>*& rootend, tsize max, auto&& callback) {
                    if (!root) {
                        return 0;
                    }
                    if (l) {
                        l->lock();
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
                    if (l) {
                        l->unlock();
                    }
                    callback(start, goal);
                    return count;
                }

               public:
                tsize steal(LinkQue<T>& by, tsize max, bool do_lock) {
                    allocate::Alloc* a = stock.get_alloc();
                    allocate::Alloc* ba = by.stock.get_alloc();
                    if (a != ba) {
                        return 0;
                    }
                    return steal_(
                        do_lock ? &stock : nullptr,
                        first, last, max,
                        [&](LinkNode<T>* b, LinkNode<T>* e) {
                            std::scoped_lock l{by.m};
                            add_node_range(by.first, by.last, b, e);
                        });
                }

                tsize steal_stock(LinkQue<T>& by, tsize max, bool do_lock) {
                    allocate::Alloc* a = stock.get_alloc();
                    allocate::Alloc* ba = by.stock.get_alloc();
                    if (a != ba) {
                        return 0;
                    }
                    auto b = stock.begin();
                    auto e = stock.end();
                    if (!b || !e) {
                        return 0;
                    }
                    return steal_(
                        do_lock ? &stock : nullptr,
                        *b, *e, max,
                        [&](LinkNode<T>* b, LinkNode<T>* e) {
                            std::scoped_lock l{by.m};
                            by.stock.set_range(b, e, a);
                        });
                }

                void free_stock(bool do_lock) {
                    auto b = stock.begin();
                    auto e = stock.end();
                    allocate::Alloc* al = stock.get_alloc();
                    if (!al || !b || !e || stock.empty()) {
                        return;
                    }
                    if (do_lock) {
                        stock.lock();
                    }
                    auto stock_begin = *b;
                    *b = nullptr;
                    *e = nullptr;
                    if (do_lock) {
                        stock.unlock();
                    }
                    for (LinkNode<T>* s = stock_begin; s;) {
                        auto n = s->next;
                        s->next = nullptr;
                        al->deallocate(s);
                        s = n;
                    }
                }

                void destruct(auto&& callback) {
                    std::scoped_lock l{stock};
                    allocate::Alloc* al = stock.get_alloc();
                    if (!al) {
                        return;
                    }
                    free_stock(false);
                    for (LinkNode<T>* s = first; s;) {
                        auto n = s->next;
                        s->next = nullptr;
                        callback(s->value);
                        al->deallocate(s);
                        s = n;
                    }
                }

                linkq_iterator<T> begin() {
                    return linkq_iterator(first);
                }

                linkq_iterator<T> end() const {
                    return linkq_iterator<T>(nullptr);
                }

                bool push_back(T&& k) {
                    return en_q(std::forward<decltype(k)>(k));
                }

                bool pop_front() {
                    T t;
                    return de_q(t);
                }

                T& front() {
                    return first->value;
                }

                T& back() {
                    return last->value;
                }

                size_t size() const {
                    size_t v = 0;
                    for (auto n = first; n; n = n->next) {
                        v++;
                    }
                    return v;
                }
            };
        }  // namespace mem
    }      // namespace quic
}  // namespace utils

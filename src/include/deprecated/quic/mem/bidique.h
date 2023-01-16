/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// bidique - bidirectional queue
#pragma once
#include <cassert>
#include <cstddef>
#include <iterator>
#include "alloc.h"

namespace utils {
    namespace quic {
        namespace mem {
            template <class T>
            struct BidiLink {
                T value;
                BidiLink<T>* prev = nullptr;
                BidiLink<T>* next = nullptr;
            };

            template <class T>
            struct BidiStock {
                BidiLink<T>* begin_ = nullptr;
                BidiLink<T>* end_ = nullptr;
                allocate::Alloc* alloc;

                void set(BidiLink<T>* b, BidiLink<T>* e) {
                    if (!begin_) {
                        assert(!end_);
                        begin_ = b;
                        end_ = e;
                    }
                    else {
                        end_->next = b;
                        e->prev = end_;
                        end_ = e;
                    }
                }

                BidiLink<T>* get() {
                    if (!begin_) {
                        assert(!end_);
                        if (alloc) {
                            return alloc->allocate<BidiLink<T>>();
                        }
                        return nullptr;
                    }
                    auto ret = begin_;
                    auto nbegin = begin_->next;
                    ret->next = nullptr;
                    if (!nbegin) {
                        assert(begin_ == end_);
                        begin_ = nullptr;
                        end_ = nullptr;
                    }
                    else {
                        nbegin->prev = nullptr;
                        begin_ = nbegin;
                    }
                    return ret;
                }

                void clean_stock() {
                    auto n = begin_;
                    while (n) {
                        auto next = n->next;
                        alloc->deallocate(n);
                        n = next;
                    }
                    begin_ = nullptr;
                    end_ = nullptr;
                }
            };

            template <class T>
            struct bidi_iterator {
               private:
                BidiLink<T>* node = nullptr;
                bidi_iterator(BidiLink<T>* n)
                    : node(n) {}
                template <class T_>
                friend struct BidiQue;

               public:
                using difference_type = std::ptrdiff_t;
                using value_type = T;
                using pointer = T*;
                using reference = T&;
                using iterator_category = std::bidirectional_iterator_tag;
                T& operator*() {
                    return node->value;
                }

                bidi_iterator& operator++() {
                    node = node->next;
                    return *this;
                }

                bidi_iterator operator++(int) {
                    auto n = node;
                    node = node->next;
                    return bidi_iterator(n);
                }

                bidi_iterator& operator--() {
                    node = node->prev;
                    return *this;
                }

                bidi_iterator operator--(int) {
                    auto n = node;
                    node = node->prev;
                    return bidi_iterator(n);
                }

                constexpr bool operator==(const bidi_iterator& o) const {
                    return node == o.node;
                }
            };

            template <class T>
            struct BidiQue {
               private:
                BidiLink<T>* first = nullptr;
                BidiLink<T>* last = nullptr;
                size_t size_ = 0;
                BidiStock<T> stock;

               public:
                void set_alloc(allocate::Alloc* a) {
                    stock.alloc = a;
                }

                void clean_stock() {
                    stock.clean_stock();
                }

                bool en_q_first_node_nlock(BidiLink<T>* node) {
                    if (!node || node->prev || node->next) {
                        return false;
                    }
                    if (!first) {
                        assert(!last);
                        first = node;
                        last = node;
                    }
                    else {
                        node->next = first;
                        first->prev = node;
                        first = node;
                    }
                    size_++;
                    return true;
                }

                bool en_q_last_node_nlock(BidiLink<T>* node) {
                    if (!node || node->prev || node->next) {
                        return false;
                    }
                    if (!first) {
                        assert(!last);
                        first = node;
                        last = node;
                    }
                    else {
                        last->next = node;
                        node->prev = last;
                        last = node;
                    }
                    size_++;
                    return true;
                }

                BidiLink<T>* de_q_first_node_nlock() {
                    if (!first) {
                        assert(!last);
                        return nullptr;
                    }
                    auto ret = first;
                    auto nfirst = first->next;
                    ret->next = nullptr;
                    if (!nfirst) {
                        assert(first == last);
                        first = nullptr;
                        last = nullptr;
                    }
                    else {
                        nfirst->prev = nullptr;
                        first = nfirst;
                    }
                    size_--;
                    return ret;
                }

                BidiLink<T>* de_q_last_node_nlock() {
                    if (!last) {
                        assert(!first);
                        return nullptr;
                    }
                    auto ret = last;
                    auto nlast = ret->prev;
                    ret->prev = nullptr;
                    if (!nlast) {
                        assert(first == last);
                        first = nullptr;
                        last = nullptr;
                    }
                    else {
                        nlast->next = nullptr;
                        last = nlast;
                    }
                    size_--;
                    return ret;
                }

                bool en_q_node_selected(BidiLink<T>* pos, BidiLink<T>* node) {
                    if (!node || node->prev || node->next) {
                        return false;
                    }
                    if (!pos) {
                        return en_q_first_node_nlock(node);
                    }
                    if (pos == last) {
                        return en_q_last_node_nlock(node);
                    }
                    for (auto n = first; n; n = n->next) {
                        if (n == pos) {
                            assert(n->prev && n->next);
                            auto next = n->next;
                            auto prev = n;
                            prev->next = node;
                            node->prev = prev;
                            node->next = next;
                            next->prev = node;
                            size_++;
                            return true;
                        }
                    }
                    return false;
                }

                bool de_q_node_selected(BidiLink<T>* pos) {
                    if (!pos) {
                        return false;
                    }
                    if (pos == first) {
                        return bool(de_q_first_node_nlock());
                    }
                    if (pos == last) {
                        return bool(de_q_last_node_nlock());
                    }
                    for (auto n = first; n; n = n->next) {
                        if (n == pos) {
                            assert(n->prev && n->next);
                            auto prev = n->prev;
                            auto next = n->next;
                            prev->next = next;
                            next->prev = prev;
                            n->prev = nullptr;
                            n->next = nullptr;
                            size_--;
                            return true;
                        }
                    }
                    return false;
                }

                bool en_q_first_nlock(T&& k) {
                    auto g = stock.get();
                    if (!g) {
                        return false;
                    }
                    g->value = std::move(k);
                    return en_q_first_node_nlock(g);
                }

                bool en_q_last_nlock(T&& k) {
                    auto g = stock.get();
                    if (!g) {
                        return false;
                    }
                    g->value = std::move(k);
                    return en_q_last_node_nlock(g);
                }

                bool de_q_first_nlock(T& k) {
                    auto g = de_q_first_node_nlock();
                    if (!g) {
                        return false;
                    }
                    k = std::move(g->value);
                    stock.set(g, g);
                    return true;
                }

                bool de_q_last_nlock(T& k) {
                    auto g = de_q_last_node_nlock();
                    if (!g) {
                        return false;
                    }
                    k = std::move(g->value);
                    stock.set(g, g);
                    return true;
                }

                bidi_iterator<T> begin() {
                    return bidi_iterator<T>(first);
                }

                bidi_iterator<T> end() const {
                    return bidi_iterator<T>(nullptr);
                }

                constexpr size_t size() const {
                    return size_;
                }

                bool push_back(T&& v) {
                    return en_q_last_nlock(std::move(v));
                }

                bool push_front(T&& v) {
                    return en_q_first_nlock(std::move(v));
                }

                bool pop_back() {
                    T t;
                    return de_q_last_nlock(t);
                }

                bool pop_front() {
                    T t;
                    return de_q_first_nlock(t);
                }

                T& back() {
                    return last->value;
                }

                T& front() {
                    return first->value;
                }

                T& operator[](size_t i) {
                    size_t c = 0;
                    for (auto n = first; n; n = n->next) {
                        if (i == c) {
                            return n->value;
                        }
                        c++;
                    }
                    int* ptr = nullptr;
                    *ptr = 0;  // terminate
                    unreachable("should be terminated");
                }
            };
        }  // namespace mem
    }      // namespace quic
}  // namespace utils

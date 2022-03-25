/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// lock_free_list - lock free list
#pragma once
#include <atomic>
#include <cassert>

namespace utils {
    namespace async {
        namespace light {
            template <class T>
            struct LFList;

            template <class T>
            struct ListElement {
                std::atomic<T> ptr;
                std::atomic<ListElement*> next;
                LFList<T>* root = nullptr;
                ListElement* get_next() const {
                    auto p = next.load();
                    if (p) {
                        return next.load();
                    }
                    return root ? root->begin.load() : nullptr;
                }

                bool try_set_task(T p) {
                    if (!p) {
                        return false;
                    }
                    T expect{};
                    if (!ptr.compare_exchange_strong(expect, p)) {
                        return false;
                    }
                    return true;
                }

                T acquire_task() {
                    return ptr.exchange(nullptr);
                }

                ~ListElement() {
                    if (ptr) {
                        std::terminate();  // invalid deletion
                    }
                    auto n = next.exchange(nullptr);
                    delete n;
                }
            };

            template <class T>
            struct LFList {
                std::atomic<ListElement<T>*> begin = nullptr;
                std::atomic<ListElement<T>*> end = nullptr;
                void insert_list(ListElement<T>* list) {
                    if (!list) {
                        return;
                    }
                    list->root = this;
                    auto old = end.exchange(list);
                    if (old) {
                        old->next.store(list);
                    }
                    else {
                        begin.store(list);
                    }
                }

                void clear() {
                    auto d = begin.exchange(nullptr);
                    end.exchange(nullptr);
                    delete d;
                }
            };

            template <class T>
            bool insert_element(LFList<T>* root, T task) {
                if (!root) {
                    return false;
                }
                auto b = root->begin.load();
                if (b) {
                    if (b->try_set_task(task)) {
                        return false;
                    }
                    for (auto p = b->get_next(); p != b; p = p->get_next()) {
                        if (p->try_set_task(task)) {
                            return false;
                        }
                    }
                }
                auto list = new ListElement<T>{};
                auto set = list->try_set_task(task);
                assert(set == true);
                root->insert_list(list);
                return true;
            }

            template <class T>
            T get_a_task(LFList<T>* root) {
                auto b = root->begin.load();
                if (b) {
                    auto t = b->acquire_task();
                    if (t) {
                        return t;
                    }
                    for (auto p = b->get_next(); p != b; p = p->get_next()) {
                        t = p->acquire_task();
                        if (t) {
                            return t;
                        }
                    }
                }
                return nullptr;
            }

        }  // namespace light
    }      // namespace async
}  // namespace utils

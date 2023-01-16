/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hash_map - hash map
#pragma once
#include "que.h"
#include "vec.h"
#include <functional>

namespace utils {
    namespace quic {
        namespace mem {
            template <class Key, class Value>
            struct KeyValue {
                Key key;
                Value value;
                constexpr KeyValue() = default;
                constexpr KeyValue(KeyValue&&) = default;
                constexpr KeyValue& operator=(KeyValue&&) = default;
                constexpr KeyValue(auto&& k, auto&& v)
                    : key(std::forward<decltype(k)>(k)), value(std::forward<decltype(v)>(v)) {}
            };

            template <class Key, class Value>
            struct HashNode {
                using kv_t = KeyValue<Key, Value>;
                tsize bugget;
                LinkQue<kv_t, SharedStock<kv_t, EmptyLock>> v;
            };

            struct equal_t {
                bool operator()(auto&& a, auto&& b) {
                    return a == b;
                }
            };

            template <class Key, class Value, class Lock = std::recursive_mutex, class Hash = std::hash<Key>, class KeyEq = equal_t>
            struct HashMap {
               private:
                Hash hash;
                KeyEq keyeq;
                using hash_t = decltype(hash(Key{}));
                using node_t = HashNode<Key, Value>;
                Vec<node_t> q;
                using KV = KeyValue<Key, Value>;
                StockNode<KV, EmptyLock> stock;
                tsize total = 0;
                Lock m;

                node_t& index(auto&& key) {
                    const hash_t khash = hash(key);
                    return q[khash % q.size()];
                }

                bool key_callback(auto&& key, auto&& callback) {
                    if (q.size() == 0) {
                        return false;
                    }
                    node_t& node = index(key);
                    bool b = false, r = false;
                    node.v.for_each_nlock([&](KV& v) {
                        if (keyeq(v.key, key)) {
                            r = callback(&v, node);
                            b = true;
                            return false;
                        }
                        return true;
                    });
                    if (b) {
                        return r;
                    }
                    return callback(nullptr, node);
                }

                auto lock_callback(auto&& callback) {
                    std::scoped_lock l{m};
                    return callback();
                }

               public:
                constexpr HashMap() = default;
                constexpr HashMap(allocate::Alloc* a)
                    : q(a) {
                    stock.a = a;
                }

                bool set_alloc(allocate::Alloc* a) {
                    if (stock.a) {
                        return false;
                    }
                    q = {a};
                    stock.a = a;
                    return true;
                }

                bool remove_nlock(auto&& key) {
                    if (q.size() == 0) {
                        return false;
                    }
                    node_t& node = index(key);
                    bool r = false;
                    node.v.rem_q_nlock([&](KV& kv) {
                        if (!r && keyeq(kv.key, key)) {
                            r = true;
                            node.bugget--;
                            total++;
                            return true;
                        }
                        return false;
                    });
                    return r;
                }

                KV* get_nlock(auto&& key) {
                    KV* ret = nullptr;
                    key_callback(key, [&](KV* kv, node_t&) {
                        ret = kv;
                        return true;
                    });
                    return ret;
                }

                bool insert_nlock(auto&& key, auto&& value) {
                    return key_callback(key, [&](KV* kv, node_t& n) {
                        if (kv) {
                            return false;
                        }
                        auto res = n.v.en_q_nlock(KV{std::forward<decltype(key)>(key), std::forward<decltype(value)>(value)});
                        if (res) {
                            n.bugget++;
                            total++;
                        }
                        return res;
                    });
                }

                bool replace_nlock(auto&& key, auto&& value) {
                    return key_callback(key, [&](KV* kv, node_t& n) {
                        if (!kv) {
                            return false;
                        }
                        kv->value = std::move(value);
                        return true;
                    });
                }

                bool replace_or_insert_nlock(auto&& key, auto&& value) {
                    return key_callback(key, [&](KV* kv, node_t& n) {
                        if (kv) {
                            kv->value = std::move(value);
                            return true;
                        }
                        auto res = n.v.en_q_nlock(KV{std::move(key), std::move(value)});
                        if (res) {
                            n.bugget++;
                            total++;
                        }
                    });
                }

                void for_each_nlock(auto&& callback) {
                    for (node_t& c : q) {
                        c.v.for_each_nlock([&](KV& kv) {
                            return callback(kv.key, kv.value);
                        });
                    }
                }

                KV* get(auto&& key) {
                    return lock_callback([&] {
                        return get_nlock(key);
                    });
                }

                bool remove(auto&& key) {
                    return lock_callback([&] {
                        return remove_nlock(key);
                    });
                }

                bool insert(auto&& key, auto&& value) {
                    return lock_callback([&] {
                        return insert_nlock(std::forward<decltype(key)>(key), std::forward<decltype(value)>(value));
                    });
                }

                bool replace_or_insert(auto&& key, auto&& value) {
                    return lock_callback([&] {
                        return replace_or_insert_nlock(key);
                    });
                }

                bool rehash_nlock(tsize s) {
                    if (q.size() >= s) {
                        return false;
                    }
                    if (!q.will_append(s)) {
                        return false;
                    }
                    // from here
                    // no allocation will occur
                    node_t tmp;
                    for (tsize i = 0; i < q.size(); i++) {
                        LinkNode<KV>* d = q[i].v.de_q_node_nlock();
                        while (d) {
                            auto b = tmp.v.en_q_node_nlock(d);
                            assert(b);
                            d = q[i].v.de_q_node_nlock();
                        }
                        q[i].bugget = 0;
                    }
                    while (q.size() < s) {
                        node_t node{};
                        node.v.stock.shared = &stock;
                        auto b = q.append(std::move(node));
                        assert(b);
                    }
                    for (LinkNode<KV>* n = tmp.v.de_q_node_nlock(); n; n = tmp.v.de_q_node_nlock()) {
                        node_t& node = index(n->value.key);
                        node.v.en_q_node_nlock(n);
                        node.bugget++;
                    }
                    return true;
                }

                bool rehash(tsize s) {
                    return lock_callback([&] {
                        return rehash_nlock(s);
                    });
                }

                void for_each(auto&& callback) {
                    return lock_callback([&] {
                        return for_each_nlock(callback);
                    });
                }

                tsize size() const {
                    return total;
                }

                float load_factor() const {
                    if (q.size() == 0) {
                        return -1;
                    }
                    return size() / q.size();
                }

                bool emplace(auto&& key, auto&& value) {
                    return insert_nlock(std::forward<decltype(key)>(key),
                                        std::forward<decltype(value)>(value));
                }

                void free_all() {
                    auto b = stock.begin();
                    auto e = stock.end();
                    auto f = *b;
                    auto l = *e;
                    *b = nullptr;
                    *e = nullptr;
                    auto n = f;
                    auto a = stock.get_alloc();
                    while (n) {
                        auto next = n->next;
                        a->deallocate(n);
                        n = next;
                    }
                    for (node_t& v : q) {
                        v.v.destruct([](auto&&) {});
                    }
                }
            };
        }  // namespace mem
    }      // namespace quic
}  // namespace utils

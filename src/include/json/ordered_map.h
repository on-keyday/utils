/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// ordered_map - ordered map
#pragma once
#include "../wrap/lite/vector.h"
#include "../helper/equal.h"

namespace utils {

    namespace json {
        template <template <class...> class Vec, class Key, class Value>
        struct OrderedMapBase {
            Vec<std::pair<Key, Value>> obj;

            template <class T>
            auto find(T&& t) const {
                return std::find_if(obj.begin(), obj.end(), [&](auto& kv) {
                    return helper::equal(std::get<0>(kv), t);
                });
            }

            auto begin() const {
                return obj.begin();
            }

            auto end() const {
                return obj.end();
            }

            template <class K, class V>
            auto emplace(K&& key, V&& value) {
                using result_t = std::pair<decltype(find(key)), bool>;
                auto e = find(key);
                if (e != end()) {
                    return result_t{e, false};
                }
                obj.push_back({key, value});
                auto ret = --obj.end();
                return result_t{ret, true};
            }

            template <class K>
            bool erase(K&& k) {
                return std::erase_if(obj, [&](auto& kv) {
                    return helper::equal(std::get<0>(kv), k);
                });
            }
            size_t size() const {
                return obj.size();
            }
        };

        template <class Key, class Value>
        using ordered_map = OrderedMapBase<wrap::vector, Key, Value>;

    }  // namespace json

}  // namespace utils
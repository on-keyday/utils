/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - define json object
#pragma once

#include "jsonbase.h"
#include "../../../wrap/lite/string.h"
#include "../../../wrap/lite/map.h"
#include "../../../wrap/lite/vector.h"
#include <utility>
#include <algorithm>
#include "../../../helper/equal.h"
#include "parse.h"
#include "to_string.h"

namespace utils {
    namespace net {
        namespace json {
            using JSON = JSONBase<wrap::string, wrap::vector, wrap::map>;

            template <template <class...> class Vec, class Key, class Value>
            struct OrderedMapBase {
                Vec<std::pair<const Key, Value>> obj;

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
            };

            template <class Key, class Value>
            using ordered_map = OrderedMapBase<wrap::vector, Key, Value>;

            using OrderedJSON = JSONBase<wrap::string, wrap::vector, ordered_map>;

            namespace literals {
                JSON operator""_json(const char* s, size_t sz) {
                    return parse<JSON>(helper::SizedView(s, sz));
                }

                OrderedJSON operator""_ojson(const char* s, size_t sz) {
                    return parse<OrderedJSON>(helper::SizedView(s, sz));
                }
            }  // namespace literals

        }  // namespace json
    }      // namespace net
}  // namespace utils

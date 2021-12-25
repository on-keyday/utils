/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/http/http1.h"
#include "../../../include/wrap/lite/string.h"
#include "../../../include/wrap/lite/map.h"
#include "../../../include/wrap/lite/vector.h"

#include "../../../include/helper/strutil.h"

#include <algorithm>

namespace utils {
    namespace net {
        namespace internal {
            struct HeaderImpl {
                wrap::vector<std::pair<wrap::string, wrap::string>> order;

                void emplace(auto&& key, auto&& value) {
                    order.emplace_back(std::move(key), std::move(value));
                }

                auto begin() {
                    return order.begin();
                }

                auto end() {
                    return order.end();
                }

                wrap::string* find(auto& key, size_t idx = 0) {
                    size_t count = 0;
                    auto found = std::find_if(order.begin(), order.end(), [&](auto& v) {
                        if (helper::equal(std::get<0>(v), key, helper::ignore_case())) {
                            if (idx == count) {
                                return true;
                            }
                            count++;
                        }
                        return false;
                    });
                    if (found != order.end()) {
                        return std::addressof(std::get<0>(*found));
                    }
                    return nullptr;
                }
            };
        }  // namespace internal

    }  // namespace net
}  // namespace utils

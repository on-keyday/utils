/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header_impl - http1 header impl
#pragma once
#include "../../../include/net/http/header.h"
#include "../../../include/wrap/lite/vector.h"
#include "../../../include/wrap/lite/string.h"
#include <algorithm>

namespace utils {
    namespace net {
        namespace internal {
            struct HeaderImpl {
                h1header::StatusCode code;
                wrap::vector<std::pair<wrap::string, wrap::string>> order;
                wrap::string body;
                wrap::string raw_header;
                bool changed = false;

                void emplace(auto&& key, auto&& value) {
                    order.emplace_back(std::move(key), std::move(value));
                    changed = true;
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
                        return std::addressof(std::get<1>(*found));
                    }
                    return nullptr;
                }
            };
        }  // namespace internal
    }      // namespace net
}  // namespace utils